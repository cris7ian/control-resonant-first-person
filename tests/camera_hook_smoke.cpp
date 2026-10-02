// Owned records and fake native functions only; this does not establish live compatibility.
#include <windows.h>
#undef near // Windows legacy pointer annotation conflicts with the assertion helper.
#include "../src/camera_override.cpp"
#include <iostream>
#include <stdexcept>

namespace {
int calls{},projection_calls{},setter_calls{};
float* output{};
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
    ++calls;output[9]=0;output[10]=0;output[11]=6;return 12345;
}
void fake_projection(void* camera,const void* transform,float* fov,float aspect) {
    ++projection_calls;check(transform==output,"projection transform argument changed");
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
        check(records && lens,"allocation failed");output=records;
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
            float fov_input=0.75f;const int before=projection_calls;
            efp::projection_detour(lens,output,&fov_input,1.777f);
            check(projection_calls==before+1 && fov_input==0.75f,"projection forwarding or input FOV changed");
            float result{};std::memcpy(&result,lens+0x2d0,4);return result;
        };
        efp::publish_camera_control(false,tick,settings,true);camera();near(output[11],6,"idle changed native output");
        check(!efp::latest_camera_telemetry().write_attempted,"idle attempted a write");
        efp::publish_camera_control(true,tick,settings,true);camera();near(output[11],6,"entry did not start at native endpoint");
        tick+=90;efp::publish_camera_control(true,tick,settings,true);camera();
        const auto target=efp::anchored_position({0,0,0},{0,0,-1},settings);check(target.has_value(),"test calibration invalid");
        near(output[11],6+((*target)[2]-6)*0.5f,"entry easing midpoint");
        near(project(),90.0f*std::numbers::pi_v<float>/180,"FOV did not use position blend");
        check(efp::latest_fov_telemetry().matched,"render camera match missing");
        tick+=90;efp::publish_camera_control(true,tick,settings,true);camera();near(output[11],(*target)[2],"entry endpoint changed calibration");
        near(project(),100.0f*std::numbers::pi_v<float>/180,"first-person FOV target");
        native_fov=85.0f*std::numbers::pi_v<float>/180;
        near(project(),105.0f*std::numbers::pi_v<float>/180,"native FOV effect not retained");
        efp::publish_camera_control(false,tick,settings,true);camera();near(output[11],(*target)[2],"manual exit snapped");
        tick+=90;efp::publish_camera_control(false,tick,settings,true);camera();near(output[11],6+((*target)[2]-6)*0.5f,"exit midpoint");
        near(project(),95.0f*std::numbers::pi_v<float>/180,"exit FOV midpoint");
        tick+=90;efp::publish_camera_control(false,tick,settings,true);camera();near(output[11],6,"exit native endpoint");near(project(),native_fov,"exit did not restore native FOV");
        settings.transition_ms=0;efp::publish_camera_control(true,tick,settings,true);camera();near(output[11],(*target)[2],"instant mode changed placement");
        project();const int setters_before=setter_calls;
        output[9]+=2;near(project(),native_fov,"unmatched camera got FOV override");check(setter_calls==setters_before,"unmatched camera called setter");
        camera();lens_mode=0;near(project(),native_fov,"nonperspective camera modified");lens_mode=1;
        native_override=1;near(project(),native_fov,"global native override superseded");native_override=0;
        lens_vtable=123;near(project(),native_fov,"unexpected camera type modified");lens_vtable=efp::render_vtable;
        camera();late_output=true;near(project(),native_fov,"late output change ignored");late_output=false;
        camera();late_safety=true;near(project(),native_fov,"late safety interruption ignored");late_safety=false;
        efp::publish_camera_control(true,tick,settings,true);camera();focused=false;near(project(),native_fov,"focus loss did not restore native FOV");
        camera();near(output[11],6,"focus loss eased instead of restoring native output");focused=true;
        mode=1;camera();near(output[11],6,"protected native mode wrote position");near(project(),native_fov,"protected mode wrote FOV");mode=0;
        efp::publish_camera_control(false,tick,settings,false);camera();near(output[11],6,"safety rollback wrote position");near(project(),native_fov,"safety rollback wrote FOV");
        settings.first_person_fov_enabled=false;efp::publish_camera_control(true,tick,settings,true);camera();near(output[11],(*target)[2],"disabled FOV blocked position");near(project(),native_fov,"disabled FOV wrote lens");
        input[5]=100;camera();check(!efp::latest_camera_telemetry().anchor_valid && efp::last_valid_record.load()==0,"invalid anchor retained eligibility");near(output[11],6,"invalid anchor wrote position");input[5]=-0.25f;
        tick+=151;camera();near(output[11],6,"stale control wrote position");near(project(),native_fov,"stale control wrote FOV");
        VirtualFree(lens,0,MEM_RELEASE);VirtualFree(records,0,MEM_RELEASE);
        std::cout << "Original forwarding, eased position/FOV, native effects, matching, independent FOV rejection and immediate safety rollback passed.\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
