#include "core.hpp"
#include "state_snapshot.hpp"
#include "camera_geometry.hpp"
#include "camera_transition.hpp"
#include <cmath>
#include <array>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace efp;
namespace {
int checks = 0;
void check(bool value, const char* expression, int line) {
    ++checks;
    if (!value) throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + expression);
}
#define CHECK(x) check((x), #x, __LINE__)
Context exploring() { return {GameState::exploration, true, true, true, true}; }
void gesture(CameraPolicy& p, Millis start, Context c = exploring()) {
    p.update(start, false, c);
    p.update(start + 10, true, c); p.update(start + 60, false, c);
    p.update(start + 130, true, c); p.update(start + 180, false, c);
}
void detector_tests() {
    DoubleTap d;
    CHECK(!d.update(0, false));
    CHECK(!d.update(10, true)); CHECK(!d.update(70, false));
    CHECK(!d.update(130, true)); CHECK(d.update(190, false));
    CHECK(!d.update(250, true)); CHECK(!d.update(310, false)); // third tap does not overlap
    CHECK(!d.update(370, true)); CHECK(d.update(430, false));
    d.reset();
    CHECK(!d.update(0, true)); CHECK(!d.update(100, false)); // held on entry is discarded
    CHECK(!d.update(170, true)); CHECK(!d.update(220, false));
    CHECK(!d.update(300, true)); CHECK(d.update(350, false));
    d.reset(); d.update(0, false);
    d.update(10, true); CHECK(!d.update(300, false)); // long hold
    d.update(370, true); CHECK(!d.update(420, false));
    d.reset(); d.update(0, false);
    d.update(10, true); d.update(60, false);
    d.update(500, true); CHECK(!d.update(550, false)); // outside window
    d.reset(); d.update(0, false);
    d.update(10, true); d.update(60, false);
    d.update(70, true); CHECK(!d.update(80, false)); // bounce
    d.update(140, true); CHECK(!d.update(190, false));
    d.reset(); d.update(0, false);
    d.update(10, true); d.update(60, false); CHECK(!d.update(40, true)); // clock went backwards
    CHECK(!d.update(100, false)); d.update(170, true); CHECK(!d.update(200, false));
    for (const Millis step : {8ULL, 16ULL, 33ULL}) {
        DoubleTap f; f.update(0, false);
        f.update(step, true); f.update(step + 66, false);
        f.update(step + 132, true); CHECK(f.update(step + 198, false));
    }
}
void policy_tests() {
    CameraPolicy p;
    gesture(p, 0); CHECK(p.active()); CHECK(p.requested());
    gesture(p, 500); CHECK(!p.active());
    gesture(p, 1000); CHECK(p.active());
    auto c = exploring(); c.state = GameState::combat;
    CHECK(!p.update(1300, false, c)); CHECK(!p.requested());
    gesture(p, 1400, c); CHECK(!p.active());
    CHECK(!p.update(1700, false, exploring())); CHECK(!p.requested()); // no automatic resume
    gesture(p, 1800); CHECK(p.active());
    c = exploring(); c.state = GameState::protected_camera;
    CHECK(!p.update(2100, false, c)); CHECK(p.requested());
    gesture(p, 2200, c); CHECK(!p.active());
    CHECK(p.update(2500, false, exploring())); // protected camera merely suspends
    for (int reason = 0; reason < 6; ++reason) {
        CameraPolicy q; gesture(q, 0); CHECK(q.active());
        c = exploring();
        if (reason == 0) c.state_fresh = false;
        if (reason == 1) c.camera_valid = false;
        if (reason == 2) c.foreground = false;
        if (reason == 3) c.controller_connected = false;
        if (reason == 4) c.state = GameState::unknown;
        if (reason == 5) c.state = GameState::combat;
        CHECK(!q.update(300, false, c)); CHECK(!q.requested());
        CHECK(!q.update(400, false, exploring()));
    }
    CameraPolicy q; q.update(0, false, exploring());
    q.update(10, true, exploring()); q.update(60, false, exploring());
    c = exploring(); c.state = GameState::combat; q.update(100, false, c);
    q.update(150, false, exploring()); q.update(160, true, exploring());
    CHECK(!q.update(210, false, exploring())); // tap cannot bridge combat
    Settings disabled; disabled.enabled = false; q.configure(disabled); gesture(q, 500); CHECK(!q.active());
    CameraPolicy r; gesture(r, 0); auto changed = Settings{}; changed.double_tap_ms = 400;
    r.configure(changed); CHECK(!r.active()); CHECK(!r.requested());
}
void geometry_tests() {
    const Settings settings;
    const Vec3 free_anchor{273.948f,1.716f,141.667f};
    const Vec3 free_secondary{273.948f,1.466f,141.667f};
    const Vec3 free_native{274.038f,1.891f,147.646f};
    const Vec3 direction{-0.032f,-0.021f,-0.999f};
    CHECK(plausible_anchor(free_anchor,free_secondary,free_native));
    const auto free_target = anchored_position(free_anchor,free_secondary,direction,settings);
    CHECK(free_target.has_value());
    for (float boom : {0.6f,1.0f,3.3f,6.0f,6.4f}) {
        Vec3 native{}; for (unsigned i=0; i<3; ++i) native[i] = free_anchor[i]-direction[i]*boom;
        CHECK(plausible_anchor(free_anchor,free_secondary,native));
    }
    // Recorded old free-space view, adjusted for the user's new height (+0.10).
    const Vec3 calibrated{273.838f,1.609f,141.351f};
    for (unsigned i=0; i<3; ++i) CHECK(std::abs((*free_target)[i]-calibrated[i]) < 0.04f);
    const Vec3 wall_anchor{266.410f,1.776f,129.045f};
    const Vec3 wall_secondary{266.410f,1.526f,129.045f};
    const Vec3 wall_native{265.832f,1.803f,129.010f};
    CHECK(plausible_anchor(wall_anchor,wall_secondary,wall_native));
    const auto wall_target = anchored_position(wall_anchor,wall_secondary,{0.998f,-0.040f,0.044f},settings);
    CHECK(wall_target.has_value());
    CHECK(std::abs((*wall_target)[0]-wall_anchor[0]) < 0.4f); // not the old 5.7-unit overshoot
    CHECK(std::abs((*wall_target)[1]-wall_anchor[1]) < 0.2f);
    CHECK(!plausible_anchor(wall_anchor,{266.410f,1.0f,129.045f},wall_native));
    CHECK(!plausible_anchor(wall_anchor,wall_secondary,{100,100,100}));
    CHECK(!anchored_position(free_anchor,free_secondary,{0,0,0},settings));
    CHECK(!anchored_position(free_anchor,free_secondary,{0,0,3},settings));
    for (const Vec3 axis : {Vec3{1,0,0},Vec3{0,1,0},Vec3{0,-1,0},Vec3{0,0,1}}) {
        const auto target = anchored_position({0,0,0},{0,-0.25f,0},axis,settings);
        CHECK(target.has_value());
        float distance = 0; for (float v : *target) { CHECK(std::isfinite(v)); distance += v*v; }
        CHECK(distance <= max_eye_displacement*max_eye_displacement);
    }
    auto unsafe = settings; unsafe.eye_side = 1;
    CHECK(!valid(unsafe)); CHECK(!anchored_position(free_anchor,free_secondary,direction,unsafe));
    auto nan = free_anchor; nan[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!anchored_position(nan,free_secondary,direction,settings));
    CHECK(!plausible_anchor(nan,free_secondary,free_native));
}
void traversal_geometry_tests() {
    const Settings settings;
    const Vec3 anchor{0,0,0}, secondary{0,-0.25f,0};
    const Vec3 forward{0,0,-1}, native{0,0,6};
    const auto floor_target = anchored_position(anchor,secondary,forward,settings);
    CHECK(floor_target.has_value());
    // Rigidly rotate the complete floor case through walls, ceilings and back.
    for (unsigned plane = 0; plane < 3; ++plane) for (int degrees = 0; degrees <= 360; degrees += 5) {
        const float angle = degrees * std::acos(-1.0f) / 180.0f;
        const auto rotate = [angle,plane](const Vec3& v) -> Vec3 {
            Vec3 rotated=v;const unsigned a=plane,b=(plane+1)%3;
            rotated[a]=v[a]*std::cos(angle)+v[b]*std::sin(angle);
            rotated[b]=-v[a]*std::sin(angle)+v[b]*std::cos(angle);
            return rotated;
        };
        const auto rotated_secondary = rotate(secondary);
        CHECK(plausible_anchor(anchor,rotated_secondary,rotate(native)));
        const auto target = anchored_position(anchor,rotated_secondary,rotate(forward),settings);
        CHECK(target.has_value());
        const auto expected = rotate(*floor_target);
        for (unsigned i=0;i<3;++i) CHECK(std::abs((*target)[i]-expected[i])<0.000001f);
        CHECK(plausible_anchor(anchor,rotated_secondary,rotate({0,0,0.6f}))); // boom retraction remains irrelevant
    }
    // Changing world location or direction magnitude must not change local placement.
    const Vec3 translation{17,-3,9};
    for (const Vec3 local_secondary : {secondary,Vec3{-0.25f,0,0},Vec3{0,0,0.25f}}) {
        const Vec3 direction{0.6f,0,-0.8f};
        const auto base=anchored_position(anchor,local_secondary,direction,settings);
        CHECK(base.has_value());
        Vec3 moved_secondary{},scaled_direction{};
        for (unsigned i=0;i<3;++i) { moved_secondary[i]=local_secondary[i]+translation[i];scaled_direction[i]=direction[i]*1.5f; }
        const auto moved=anchored_position(translation,moved_secondary,direction,settings);
        const auto scaled=anchored_position(anchor,local_secondary,scaled_direction,settings);
        CHECK(moved.has_value());CHECK(scaled.has_value());
        for (unsigned i=0;i<3;++i) {
            CHECK(std::abs((*moved)[i]-((*base)[i]+translation[i]))<0.00001f);
            CHECK(std::abs((*scaled)[i]-(*base)[i])<0.000001f);
        }
    }
    // Independently calculated endpoint: exercises non-default height/side/forward calibration.
    auto calibrated=settings;calibrated.prototype_distance=-6.5f;calibrated.eye_height=0.2f;
    calibrated.eye_side=-0.2f;calibrated.eye_forward=0.1f;
    const auto calibrated_target=anchored_position(anchor,secondary,forward,calibrated);
    CHECK(calibrated_target.has_value());
    const Vec3 expected_calibration{0.1f,0.25f,-0.6f};
    for (unsigned i=0;i<3;++i) CHECK(std::abs((*calibrated_target)[i]-expected_calibration[i])<0.000001f);
    const Vec3 wall_secondary{-0.25f,0,0}; // local up is +X
    const auto wall_target = anchored_position(anchor,wall_secondary,forward,settings);
    CHECK(wall_target.has_value());
    CHECK(std::abs((*wall_target)[0]-(reference_height+settings.eye_height))<0.000001f);
    CHECK(std::abs((*wall_target)[1]-(reference_side+settings.eye_side))<0.000001f);
    // Singular and near-singular views must stay finite without a world-up fallback.
    for (const Vec3 direction : {Vec3{1,0,0},Vec3{-1,0,0},Vec3{1,0.001f,0},Vec3{1,0.02f,0}}) {
        const auto target = anchored_position(anchor,wall_secondary,direction,settings);
        CHECK(target.has_value());
        float distance=0; for (float value:*target) { CHECK(std::isfinite(value)); distance+=value*value; }
        CHECK(distance<=max_eye_displacement*max_eye_displacement);
        const float length=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]);
        const float axial=-(reference_boom+settings.prototype_distance-settings.eye_forward);
        CHECK(std::abs((*target)[1]-direction[1]/length*axial)<0.000001f); // lateral contribution is along Z
    }
    struct Capture { Vec3 anchor,secondary,native,direction; };
    // Rounded geometry from the rejected wall walk and its floor-return frame.
    for (const Capture capture : {
        Capture{{-390.203f,103.208f,-545.270f},{-390.453f,103.208f,-545.270f},
            {-389.670f,97.432f,-546.814f},{-0.081f,0.958f,0.274f}},
        Capture{{-390.242f,104.027f,-540.698f},{-390.492f,104.027f,-540.698f},
            {-390.367f,109.278f,-543.684f},{0.029f,-0.866f,0.500f}},
        Capture{{-388.672f,96.952f,-544.257f},{-388.675f,96.702f,-544.257f},
            {-389.325f,102.841f,-543.920f},{0.152f,-0.988f,-0.013f}}}) {
        CHECK(plausible_anchor(capture.anchor,capture.secondary,capture.native));
        const auto target=anchored_position(capture.anchor,capture.secondary,capture.direction,settings);
        CHECK(target.has_value());
        float distance=0; for (unsigned i=0;i<3;++i) { const float d=(*target)[i]-capture.anchor[i];distance+=d*d; }
        CHECK(distance<=max_eye_displacement*max_eye_displacement);
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    for (const Vec3 bad : {anchor,Vec3{-0.1f,0,0},Vec3{-0.4f,0,0},
        Vec3{-0.25f,-0.25f,0},Vec3{nan,0,0},Vec3{0,inf,0},Vec3{1000001,0,0}}) {
        CHECK(!plausible_anchor(anchor,bad,native));
        CHECK(!anchored_position(anchor,bad,forward,settings));
    }
    for (unsigned axis=0;axis<3;++axis) for (float sign : {-1.0f,1.0f}) {
        for (float spacing : {0.15f,0.25f,0.35f}) {
            Vec3 second{};second[axis]=sign*spacing;
            CHECK(plausible_anchor(anchor,second,native));
            CHECK(anchored_position(anchor,second,forward,settings).has_value());
        }
        for (float spacing : {std::nextafter(0.15f,0.0f),std::nextafter(0.35f,1.0f)}) {
            Vec3 second{};second[axis]=sign*spacing;
            CHECK(!plausible_anchor(anchor,second,native));
            CHECK(!anchored_position(anchor,second,forward,settings));
        }
    }
    CHECK(plausible_anchor(anchor,wall_secondary,{0,0,12.0f}));
    CHECK(!plausible_anchor(anchor,wall_secondary,{0,0,12.01f}));
    CHECK(!plausible_anchor(anchor,wall_secondary,{nan,0,0}));
    CHECK(!anchored_position(anchor,wall_secondary,{0,nan,0},settings));
    auto unsafe=settings;unsafe.eye_height=2;
    CHECK(!anchored_position(anchor,wall_secondary,forward,unsafe));
}
struct MemoryFixture {
    std::vector<unsigned char> memory = std::vector<unsigned char>(0x4000);
    template<class T> void put(std::uintptr_t address, T value) {
        std::memcpy(memory.data() + address - 0x10000, &value, sizeof(value));
    }
    void text(std::uintptr_t address, const std::string& value) {
        put<std::uint32_t>(address, 0x04000004); put<std::uint32_t>(address + 4, static_cast<std::uint32_t>(value.size()));
        std::memcpy(memory.data() + address - 0x10000 + 8, value.data(), value.size());
    }
    bool read(std::uintptr_t address, void* out, std::size_t size) const {
        if (address < 0x10000 || address - 0x10000 > memory.size() || size > memory.size() - (address - 0x10000)) return false;
        std::memcpy(out, memory.data() + address - 0x10000, size); return true;
    }
    StateSnapshot snapshot() { return read_state_snapshot(0x10000, [this](auto a, auto b, auto c){ return read(a,b,c); }); }
    MemoryFixture() {
        put<std::uintptr_t>(0x10000, 0x10100); put<std::uintptr_t>(0x10100, 0x10200); put<std::uint32_t>(0x10108, 2);
        text(0x10200, "game"); text(0x102f0, "program_flow");
        put<unsigned char>(0x102b8, 1); put<std::int32_t>(0x102bc, 0);
        put<std::uintptr_t>(0x102c0, 0x11000); put<std::uint32_t>(0x102c8, 1);
        put<std::int32_t>(0x103ac, 0); put<std::uintptr_t>(0x103b0, 0x12000); put<std::uint32_t>(0x103b8, 1);
        text(0x11000, "exploration"); text(0x12000, "game");
    }
};
void snapshot_tests() {
    MemoryFixture f;
    auto s = f.snapshot(); CHECK(s.readable); CHECK(s.game_found); CHECK(s.program_found);
    CHECK(s.game_top == "exploration"); CHECK(s.program_top == "game");
    CHECK(diagnostic_state(s, 0) == GameState::exploration);
    CHECK(diagnostic_state(s, 3) == GameState::protected_camera);
    MemoryFixture inherited;
    inherited.put<unsigned char>(0x102b8, 0); // game inherits activation, matching the live log
    inherited.put<unsigned char>(0x103a8, 1); // program_flow is explicitly active and points to game
    auto child = inherited.snapshot();
    CHECK(child.readable); CHECK(!child.explicit_game_active); CHECK(child.effective_game_active);
    CHECK(diagnostic_state(child, 0) == GameState::exploration);
    inherited.put<unsigned char>(0x103a8, 0);
    CHECK(!inherited.snapshot().effective_game_active);
    CHECK(diagnostic_state(inherited.snapshot(), 0) == GameState::protected_camera);
    inherited.text(0x11000, "program_flow"); // unrooted cycle must not recurse indefinitely
    CHECK(!inherited.snapshot().effective_game_active);
    MemoryFixture story;
    story.text(0x11000, "story"); auto hub = story.snapshot();
    CHECK(diagnostic_state(hub,0) == GameState::exploration);
    CHECK(diagnostic_state(hub,1) == GameState::protected_camera);
    for (const auto overlay : {"dialogue","conversation","skippable_timeline","system_menu","map"}) {
        story.put<std::uint32_t>(0x102c8,2); story.put<std::int32_t>(0x102bc,1);
        story.text(0x11028,overlay);
        CHECK(diagnostic_state(story.snapshot(),0) == GameState::protected_camera);
    }
    story.text(0x11028,"combat"); CHECK(diagnostic_state(story.snapshot(),0) == GameState::combat);
    story.text(0x11028,"story"); CHECK(diagnostic_state(story.snapshot(),0) == GameState::protected_camera);
    f.text(0x11000, "combat"); s = f.snapshot(); CHECK(s.contains_combat); CHECK(diagnostic_state(s, 0) == GameState::combat);
    f.put<std::uint32_t>(0x102c8, 2); f.put<std::int32_t>(0x102bc, 1); f.text(0x11028, "system_menu");
    s = f.snapshot(); CHECK(s.game_top == "system_menu"); CHECK(s.contains_combat);
    CHECK(diagnostic_state(s, 0) == GameState::combat); // combat under a menu clears request
    f.text(0x11000, "exploration"); s = f.snapshot(); CHECK(diagnostic_state(s, 0) == GameState::protected_camera);
    f.put<std::int32_t>(0x102bc, -1); CHECK(!f.snapshot().readable);
    f.put<std::int32_t>(0x102bc, 9); CHECK(!f.snapshot().readable);
    f.put<std::int32_t>(0x102bc, 0); f.put<std::uint32_t>(0x102c8, 65); CHECK(!f.snapshot().readable);
    f.put<std::uint32_t>(0x102c8, 1); f.put<std::uint32_t>(0x10108, 257); CHECK(!f.snapshot().readable);
    f.put<std::uint32_t>(0x10108, 2); f.put<std::uint32_t>(0x10204, 1000); CHECK(!f.snapshot().readable);
    f.text(0x10200, "game");
    // Heap string representation: encoded threshold zero, scaled size nonzero.
    f.put<std::uint32_t>(0x10200, 1); f.put<std::uintptr_t>(0x10208, 0x13000);
    std::memcpy(f.memory.data() + 0x3000, "game", 4); CHECK(f.snapshot().readable);
    f.put<std::uintptr_t>(0x10208, std::numeric_limits<std::uintptr_t>::max()); CHECK(!f.snapshot().readable);
    CHECK(!read_state_snapshot(0, [](auto,auto,auto){return false;}).readable);
    MemoryFixture torn;
    int header_reads = 0;
    auto inconsistent = read_state_snapshot(0x10000, [&](auto a, auto b, auto c) {
        const bool ok = torn.read(a,b,c);
        if (a == 0x10100 && ++header_reads == 2) static_cast<unsigned char*>(b)[8] = 3;
        return ok;
    });
    CHECK(!inconsistent.readable);
    MemoryFixture changing_states;
    bool switched = false;
    auto mixed = read_state_snapshot(0x10000, [&](auto a, auto b, auto c) {
        if (a == 0x12000 && !switched) {
            switched = true;
            changing_states.text(0x11000, "combat"); changing_states.text(0x12000, "main_menu");
        }
        return changing_states.read(a,b,c);
    });
    CHECK(!mixed.readable); // arrays changed while pointers/indices remained unchanged
    MemoryFixture changing_heap;
    changing_heap.put<std::uint32_t>(0x11000, 1); changing_heap.put<std::uintptr_t>(0x11008, 0x13000);
    std::memcpy(changing_heap.memory.data() + 0x3000, "exploration", 11);
    int heap_reads = 0;
    auto heap_mixed = read_state_snapshot(0x10000, [&](auto a, auto b, auto c) {
        const bool ok = changing_heap.read(a,b,c);
        if (a == 0x13000 && ++heap_reads == 1) std::memcpy(changing_heap.memory.data() + 0x3000, "system_menu", 11);
        return ok;
    });
    CHECK(!heap_mixed.readable); // stable heap pointer/metadata, changed string contents
    auto absent = StateSnapshot{}; CHECK(diagnostic_state(absent, 0) == GameState::unknown);
}
void transition_tests() {
    CameraTransition t;
    CHECK(t.update(1000,true,180)==0);
    CHECK(std::abs(t.update(1090,true,180)-0.5f)<0.000001f);
    CHECK(t.update(1180,true,180)==1);
    CHECK(t.update(1200,false,180)==1);
    CHECK(std::abs(t.update(1290,false,180)-0.5f)<0.000001f);
    CHECK(t.update(1380,false,180)==0);
    t.reset();CHECK(t.update(1000,true,180)==0);
    const auto halfway=t.update(1090,true,180);
    CHECK(t.update(1090,false,180)==halfway);CHECK(t.update(1180,false,180)==0);
    t.reset();CHECK(t.update(2000,true,0)==1);CHECK(t.update(2000,false,0)==0);
    CHECK(t.update(1999,true,180)==0); // clock regression cannot reuse a stale blend
    CHECK(t.update(2200,true,std::numeric_limits<float>::quiet_NaN())==0);
    for (Millis frame : {8ULL,16ULL,33ULL}) {
        t.reset();t.update(1000,true,180);float previous=0;
        for (Millis now=1000;now<=1210;now+=frame) { const float value=t.update(now,true,180);CHECK(value>=previous && value<=1);previous=value; }
        CHECK(t.update(1210,true,180)==1);
    }
    auto c=exploring();CHECK(camera_transition_allowed(Settings{},c));
    for (int i=0;i<7;++i) {
        c=exploring();auto s=Settings{};
        if (i==0)c.state=GameState::combat;
        if (i==1)c.state=GameState::protected_camera;
        if (i==2)c.state_fresh=false;
        if (i==3)c.camera_valid=false;
        if (i==4)c.foreground=false;
        if (i==5)c.controller_connected=false;
        if (i==6)s.enabled=false;
        CHECK(!camera_transition_allowed(s,c));
    }
}
void config_tests() {
    CHECK(valid(Settings{}));
    CHECK(parse_settings("[Settings]\ntransition_ms=0\nfirst_person_fov=110\nfirst_person_fov_enabled=0")->first_person_fov==110);
    CHECK(!parse_settings("[Settings]\ntransition_ms=501"));
    CHECK(!parse_settings("[Settings]\nfirst_person_fov=59"));
    CHECK(!parse_settings("[Settings]\nfirst_person_fov=121"));
    CHECK(!parse_settings("[Settings]\nfirst_person_fov_enabled=2"));
    auto s = parse_settings("[Settings]\nenabled=1\npad_button=267\ndouble_tap_ms=400\neye_height=0.05\n");
    CHECK(s.has_value()); CHECK(s->double_tap_ms == 400); CHECK(s->eye_height == 0.05f);
    CHECK(!parse_settings("")); CHECK(!parse_settings("[Settings]\n"));
    CHECK(!parse_settings("[Settings]\nenabled="));
    CHECK(!parse_settings("[Settings]\nenabled=2"));
    CHECK(!parse_settings("[Settings]\npad_button=128")); // not an XInput mask
    CHECK(!parse_settings("[Settings]\npad_button=279"));
    CHECK(!parse_settings("[Settings]\nkeyboard_key=255"));
    CHECK(!parse_settings("[Settings]\ndouble_tap_ms=-1"));
    CHECK(!parse_settings("[Settings]\ndouble_tap_ms=999999999999999999999"));
    CHECK(!parse_settings("[Settings]\ndouble_tap_ms=350x"));
    CHECK(!parse_settings("[Settings]\neye_height=nan"));
    CHECK(!parse_settings("[Settings]\neye_height=inf"));
    CHECK(!parse_settings("[Settings]\neye_forward=0.9"));
    CHECK(parse_settings("[Settings]\nprototype_distance=-7")->prototype_distance == -7.0f);
    CHECK(parse_settings("[Settings]\nprototype_distance=-5")->prototype_distance == -5.0f);
    CHECK(parse_settings("[Settings]\nprototype_distance=-6")->prototype_distance == -6.0f);
    CHECK(!parse_settings("[Settings]\nprototype_distance=-7.01"));
    CHECK(!parse_settings("[Settings]\nprototype_distance=-4.99"));
    CHECK(!parse_settings("[Settings]\neye_side=1")); // combined local offset budget
    CHECK(!parse_settings("[Settings]\nenabled=1\nenabled=0"));
    CHECK(parse_settings("\xEF\xBB\xBF[Settings]\r\n enabled = 0 \r\n;comment\n")->enabled == false);
    auto base = Settings{}; base.double_tap_ms = 450;
    CHECK(parse_settings("[Settings]\ndebug=1", base)->double_tap_ms == 450);
    CHECK(parse_settings("[Settings]\npad_button=0\nkeyboard_key=88")->keyboard_key == 88);
    CHECK(!parse_settings("[Settings]\nmax_tap_ms=0"));
    base.eye_height = std::numeric_limits<float>::quiet_NaN(); CHECK(!valid(base));
}
}
int main() {
    try { detector_tests(); policy_tests(); config_tests(); snapshot_tests(); geometry_tests(); traversal_geometry_tests(); transition_tests(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return EXIT_FAILURE; }
    std::cout << checks << " checks passed\n";
}
