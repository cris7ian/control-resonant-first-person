// Owned records and fake native functions only; this does not establish live compatibility.
#include <windows.h>
#undef near // Windows legacy pointer annotation conflicts with the assertion helper.
#include "../src/camera_override.cpp"
#include <iostream>
#include <stdexcept>

namespace {
int calls{},projection_calls{},setter_calls{};
float* output{};
float native_boom=6;
const void* expected_frame{};
efp::Millis tick=1000;
bool focused=true,late_safety=false,late_output=false;
float native_fov=80.0f*std::numbers::pi_v<float>/180.0f;
int lens_mode=1;
std::uintptr_t lens_vtable=0x11223344;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
void near(float a,float b,const char* message) { check(std::abs(a-b)<0.0001f,message); }
std::uintptr_t fake_original(void*,std::uintptr_t a2,std::uintptr_t a3,std::uintptr_t a4,std::uintptr_t a5,
    std::uintptr_t a6,std::uintptr_t a7,std::uintptr_t a8,std::uintptr_t a9,std::uintptr_t a10) {
    check(a2==2 && a3==3 && a4==4 && a5==5 && a6==6 && a7==7 && a8==8 && a9==9 && a10==10,"argument forwarding changed");
    ++calls;output[9]=0;output[10]=0;output[11]=native_boom;return 12345;
}
void fake_projection(void* camera,const void* transform,float* fov,float aspect) {
    ++projection_calls;check(transform==expected_frame,"projection transform argument changed");
    check(fov && *fov==0.75f && aspect==1.777f,"mixed pointer/float projection ABI changed");
    auto* bytes=static_cast<unsigned char*>(camera);
    std::memcpy(bytes,&lens_vtable,8);std::memcpy(bytes+0x2cc,&lens_mode,4);
    std::memcpy(bytes+0x2d0,&native_fov,4);std::memcpy(bytes+0x2d4,&aspect,4);
    if (late_safety) efp::publish_camera_control(false,tick,efp::Settings{},false);
    if (late_output) output[9]+=1;
}
void fake_setter(void* camera,float radians) {
    ++setter_calls;std::memcpy(static_cast<unsigned char*>(camera)+0x2d0,&radians,4);
}
}
int main() {
    try {
        auto* records=static_cast<float*>(VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        auto* lens=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        auto* other_lens=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        check(records && lens && other_lens,"allocation failed");output=records;
        output[6]=0;output[7]=0;output[8]=-1;
        float input[12]{};input[5]=-0.25f;
        alignas(16) unsigned char owner[0x70]{};
        const auto input_base=reinterpret_cast<std::uintptr_t>(input),output_base=reinterpret_cast<std::uintptr_t>(output);
        std::memcpy(owner+0x30,&input_base,8);std::memcpy(owner+0x38,&output_base,8);
        int mode=0;unsigned char native_override=0;
        efp::mode_address=reinterpret_cast<std::uintptr_t>(&mode);
        efp::override_flag_address=reinterpret_cast<std::uintptr_t>(&native_override);
        efp::render_camera_address=reinterpret_cast<std::uintptr_t>(lens);efp::render_vtable=lens_vtable;
        efp::original=&fake_original;efp::original_projection=&fake_projection;efp::set_render_fov=&fake_setter;
        efp::clock_now=[] { return tick; };efp::foreground_check=[] { return focused; };
        efp::Settings settings;settings.debug=true;
        const auto camera=[&] {
            const int before=calls;
            check(efp::camera_detour(owner,2,3,4,5,6,7,8,9,10)==12345 && calls==before+1,"original result/call count changed");
        };
        const auto project=[&] {
            // The native FOV reader copies its CameraView transform onto the stack before rendering.
            std::array<float,12> frame{};std::memcpy(frame.data(),output,sizeof(frame));expected_frame=frame.data();
            float fov_input=0.75f;const int before=projection_calls;
            efp::projection_detour(lens,frame.data(),&fov_input,1.777f);
            check(projection_calls==before+1 && fov_input==0.75f,"projection forwarding or input FOV changed");
            float result{};std::memcpy(&result,lens+0x2d0,4);return result;
        };
        efp::publish_camera_control(false,tick,settings,true);camera();near(output[11],6,"idle changed native output");
        check(!efp::latest_camera_telemetry().write_attempted,"idle attempted a write");
        efp::publish_camera_control(true,tick,settings,true);camera();near(output[11],6,"entry did not start at native endpoint");
        tick+=90;efp::publish_camera_control(true,tick,settings,true);camera();
        const auto target=efp::anchored_position({0,0,0},{0,-0.25f,0},{0,0,-1},settings);check(target.has_value(),"test calibration invalid");
        near(output[11],6+((*target)[2]-6)*0.5f,"entry easing midpoint");
        near(project(),90.0f*std::numbers::pi_v<float>/180,"FOV did not use position blend");
        check(efp::latest_fov_telemetry().matched,"render camera match missing");
        tick+=90;efp::publish_camera_control(true,tick,settings,true);camera();near(output[11],(*target)[2],"entry endpoint changed calibration");
        near(project(),100.0f*std::numbers::pi_v<float>/180,"first-person FOV target");
        // Native callers pass a temporary stack query, not a persistent owner object.
        std::array<unsigned char,sizeof(owner)> owner_copy{};
        std::memcpy(owner_copy.data(),owner,sizeof(owner));std::memset(owner,0,sizeof(owner));
        near(project(),100.0f*std::numbers::pi_v<float>/180,"expired stack query blocked a live positioned record");
        std::memcpy(owner,owner_copy.data(),sizeof(owner));
        native_fov=85.0f*std::numbers::pi_v<float>/180;
        float other_input=0.75f;const int before_other=projection_calls,before_other_setter=setter_calls;
        expected_frame=output;efp::projection_detour(other_lens,output,&other_input,1.777f);
        check(projection_calls==before_other+1 && setter_calls==before_other_setter && other_input==0.75f,"unrelated render forwarding changed");
        near(project(),105.0f*std::numbers::pi_v<float>/180,"unrelated render reset the entry FOV reference");
        efp::publish_camera_control(false,tick,settings,true);camera();near(output[11],(*target)[2],"manual exit snapped");
        tick+=90;efp::publish_camera_control(false,tick,settings,true);camera();near(output[11],6+((*target)[2]-6)*0.5f,"exit midpoint");
        near(project(),95.0f*std::numbers::pi_v<float>/180,"exit FOV midpoint");
        tick+=90;efp::publish_camera_control(false,tick,settings,true);camera();near(output[11],6,"exit native endpoint");near(project(),native_fov,"exit did not restore native FOV");
        settings.transition_ms=0;efp::publish_camera_control(true,tick,settings,true);camera();near(output[11],(*target)[2],"instant mode changed placement");
        // Exercise actual hook output while the original supplies different collision-shortened booms.
        for (float boom : {0.6f,1.0f,3.3f,6.0f,6.4f}) {
            native_boom=boom;camera();
            near(efp::latest_camera_telemetry().native_position[2],boom,"fake native boom did not change");
            for (unsigned i=0;i<3;++i) near(output[9+i],(*target)[i],"native retraction moved the first-person endpoint");
        }
        native_boom=6;camera();
        project();const int setters_before=setter_calls;
        output[9]+=2;near(project(),native_fov,"unmatched camera got FOV override");check(setter_calls==setters_before,"unmatched camera called setter");
        camera();
        efp::PositionedCamera duplicate{};duplicate.address=output_base+0x40;duplicate.observed=tick;
        duplicate.epoch=efp::control.epoch;duplicate.blend=1;
        for (unsigned i=0;i<3;++i) { duplicate.position[i]=output[9+i];duplicate.direction[i]=output[6+i]; }
        efp::publish_position(duplicate);near(project(),native_fov,"ambiguous render match modified FOV");
        check(std::string(efp::latest_fov_telemetry().reason)=="ambiguous positioned camera","ambiguity was not identified");
        for (auto& p:efp::positioned) if (p.address==duplicate.address) p={};
        lens_mode=0;near(project(),native_fov,"nonperspective camera modified");lens_mode=1;
        native_override=1;near(project(),native_fov,"global native override superseded");native_override=0;
        lens_vtable=123;near(project(),native_fov,"unexpected camera type modified");lens_vtable=efp::render_vtable;
        camera();late_output=true;near(project(),native_fov,"live record changed after native transform copy");late_output=false;
        check(std::string(efp::latest_fov_telemetry().reason)=="positioned camera changed","live record revalidation not exercised");
        camera();DWORD previous_protection{};
        check(VirtualProtect(records,0x1000,PAGE_READONLY,&previous_protection)!=0,"record protection failed");
        near(project(),native_fov,"nonwritable live record got FOV override");
        check(VirtualProtect(records,0x1000,previous_protection,&previous_protection)!=0,"record protection restore failed");
        tick+=101;near(project(),native_fov,"stale positioned record got FOV override");
        efp::publish_camera_control(true,tick,settings,true);
        camera();late_safety=true;near(project(),native_fov,"late safety interruption ignored");late_safety=false;
        efp::publish_camera_control(true,tick,settings,true);camera();focused=false;near(project(),native_fov,"focus loss did not restore native FOV");
        camera();near(output[11],6,"focus loss eased instead of restoring native output");focused=true;
        mode=1;camera();near(output[11],6,"protected native mode wrote position");near(project(),native_fov,"protected mode wrote FOV");mode=0;
        efp::publish_camera_control(false,tick,settings,false);camera();near(output[11],6,"safety rollback wrote position");near(project(),native_fov,"safety rollback wrote FOV");
        native_fov=100.0f*std::numbers::pi_v<float>/180;
        efp::publish_camera_control(true,tick,settings,true);camera();
        const int before_noop=setter_calls;near(project(),native_fov,"equal native/requested FOV changed");
        check(setter_calls==before_noop && efp::latest_fov_telemetry().matched,"no-op FOV rebuilt the projection or lost the match");
        native_fov=105.0f*std::numbers::pi_v<float>/180;near(project(),native_fov,"native effect changed with zero FOV offset");
        check(setter_calls==before_noop,"zero FOV offset rebuilt the projection");
        settings.first_person_fov_enabled=false;efp::publish_camera_control(true,tick,settings,true);camera();near(output[11],(*target)[2],"disabled FOV blocked position");near(project(),native_fov,"disabled FOV wrote lens");
        settings.first_person_fov_enabled=true;
        // One active intent survives floor -> wall -> floor. Native rotation is never written.
        efp::publish_camera_control(true,tick,settings,true);
        const auto traversal_epoch=efp::control.epoch;
        input[4]=-0.25f;input[5]=0;
        camera();
        check(efp::last_valid_record.load()==tick && efp::latest_camera_telemetry().anchor_used,"wall pair rejected");
        near(output[9],efp::reference_height+settings.eye_height,"wall height used world up");
        near(output[10],efp::reference_side+settings.eye_side,"wall lateral offset used world up");
        near(output[11],(*target)[2],"wall forward calibration changed");
        near(output[6],0,"wall traversal changed native direction X");near(output[7],0,"wall traversal changed native direction Y");near(output[8],-1,"wall traversal changed native direction Z");
        near(project(),100.0f*std::numbers::pi_v<float>/180,"wall position did not match scoped FOV");
        input[4]=0;input[5]=-0.25f;camera();
        for (unsigned i=0;i<3;++i) near(output[9+i],(*target)[i],"floor return changed calibration");
        check(efp::control.active && efp::control.epoch==traversal_epoch,"wall transition lost active intent");
        input[4]=-0.25f;input[5]=0;
        mode=1;camera();near(output[11],6,"protected wall camera wrote position");near(project(),native_fov,"protected wall camera wrote FOV");mode=0;
        focused=false;camera();near(output[11],6,"unfocused wall camera wrote position");near(project(),native_fov,"unfocused wall camera wrote FOV");focused=true;
        efp::publish_camera_control(false,tick,settings,false);camera();near(output[11],6,"wall safety interruption wrote position");near(project(),native_fov,"wall safety interruption wrote FOV");
        input[4]=0;input[5]=-0.25f;
        efp::publish_camera_control(true,tick,settings,true);
        input[5]=100;camera();check(!efp::latest_camera_telemetry().anchor_valid && efp::last_valid_record.load()==0,"invalid anchor retained eligibility");near(output[11],6,"invalid anchor wrote position");input[5]=-0.25f;
        tick+=151;camera();near(output[11],6,"stale control wrote position");near(project(),native_fov,"stale control wrote FOV");
        VirtualFree(other_lens,0,MEM_RELEASE);VirtualFree(lens,0,MEM_RELEASE);VirtualFree(records,0,MEM_RELEASE);
        std::cout << "Original forwarding, eased position/FOV, native effects, matching, independent FOV rejection and immediate safety rollback passed.\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
