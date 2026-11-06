// julicans nano basics - Aufbau des Korg nanoKONTROL2: Positionen in Millimetern und die Standard-CCs im CC-Modus.
#pragma once
#include <array>
#include <cstddef>
#include <cmath>

namespace nb
{
enum Kind { KNOB = 0, FADER, BUTTON };

struct Ctl
{
    const char* id;       // kurzer Name fuer Parameter und Skript
    const char* label;    // Anzeige
    int kind;
    float x, y, w, h;     // Mitte und Groesse in mm, Ursprung in der Geraetemitte, y nach oben
    int cc;               // Standard im CC-Modus
    bool led;             // hat eine LED
    bool reserved;        // Track-Tasten schalten die Presets um
};

constexpr float BODY_W = 325.0f, BODY_H = 83.0f;
constexpr int NUM_PRESETS = 8, FIXED_BANK = NUM_PRESETS, NUM_BANKS = NUM_PRESETS + 1;
constexpr int NUM_CTL = 51;

inline const std::array<Ctl, NUM_CTL>& controls()
{
    static const std::array<Ctl, NUM_CTL> c = [] {
        std::array<Ctl, NUM_CTL> a {};
        int i = 0;
        const auto add = [&] (Ctl x) { a[(std::size_t) i++] = x; };
        add ({ "trackL", "Track zurück", BUTTON, -142.5f, 8.0f, 10.0f, 5.5f, 58, false, true });
        add ({ "trackR", "Track vor", BUTTON, -127.5f, 8.0f, 10.0f, 5.5f, 59, false, true });
        add ({ "cycle", "Cycle", BUTTON, -142.5f, -6.0f, 10.0f, 5.5f, 46, true, false });
        add ({ "set", "Marker Set", BUTTON, -107.5f, -6.0f, 10.0f, 5.5f, 60, false, false });
        add ({ "markL", "Marker zurück", BUTTON, -93.5f, -6.0f, 10.0f, 5.5f, 61, false, false });
        add ({ "markR", "Marker vor", BUTTON, -79.5f, -6.0f, 10.0f, 5.5f, 62, false, false });
        static const char* tid[5] = { "rew", "ff", "stop", "play", "rec" };
        static const char* tlb[5] = { "Zurückspulen", "Vorspulen", "Stop", "Play", "Aufnahme" };
        static const int tcc[5] = { 43, 44, 42, 41, 45 };
        for (int t = 0; t < 5; ++t) add ({ tid[t], tlb[t], BUTTON, -139.5f + (float) t * 17.0f, -25.0f, 13.0f, 7.5f, tcc[t], true, false });
        static const char* kid[8] = { "knob1", "knob2", "knob3", "knob4", "knob5", "knob6", "knob7", "knob8" };
        static const char* klb[8] = { "Drehregler 1", "Drehregler 2", "Drehregler 3", "Drehregler 4", "Drehregler 5", "Drehregler 6", "Drehregler 7", "Drehregler 8" };
        static const char* sid[8] = { "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8" };
        static const char* slb[8] = { "S 1", "S 2", "S 3", "S 4", "S 5", "S 6", "S 7", "S 8" };
        static const char* mid[8] = { "m1", "m2", "m3", "m4", "m5", "m6", "m7", "m8" };
        static const char* mlb[8] = { "M 1", "M 2", "M 3", "M 4", "M 5", "M 6", "M 7", "M 8" };
        static const char* rid[8] = { "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8" };
        static const char* rlb[8] = { "R 1", "R 2", "R 3", "R 4", "R 5", "R 6", "R 7", "R 8" };
        static const char* fid[8] = { "fader1", "fader2", "fader3", "fader4", "fader5", "fader6", "fader7", "fader8" };
        static const char* flb[8] = { "Fader 1", "Fader 2", "Fader 3", "Fader 4", "Fader 5", "Fader 6", "Fader 7", "Fader 8" };
        const float x0 = BODY_W * 0.5f - 7.0f - 8.0f * 27.0f;
        for (int ch = 0; ch < 8; ++ch)
        {
            const float cx = x0 + 27.0f * ((float) ch + 0.5f);
            add ({ kid[ch], klb[ch], KNOB, cx, 26.0f, 11.2f, 11.2f, 16 + ch, false, false });
            add ({ sid[ch], slb[ch], BUTTON, cx - 6.0f, 8.0f, 7.5f, 5.2f, 32 + ch, true, false });
            add ({ mid[ch], mlb[ch], BUTTON, cx - 6.0f, -5.0f, 7.5f, 5.2f, 48 + ch, true, false });
            add ({ rid[ch], rlb[ch], BUTTON, cx - 6.0f, -18.0f, 7.5f, 5.2f, 64 + ch, true, false });
            add ({ fid[ch], flb[ch], FADER, cx + 6.0f, -6.0f, 9.0f, 50.0f, ch, false, false });
        }
        return a;
    }();
    return c;
}

/** Index des Reglers zu einer CC-Nummer, -1 wenn keiner passt. */
inline int controlForCc (int cc)
{
    static const std::array<int, 128> map = [] {
        std::array<int, 128> m {}; m.fill (-1);
        for (int i = 0; i < NUM_CTL; ++i) m[(std::size_t) controls()[(std::size_t) i].cc] = i;
        return m;
    }();
    return cc >= 0 && cc < 128 ? map[(std::size_t) cc] : -1;
}

/** Was am Anfang fest ist: Transport und Marker, damit FL-Steuerung in jedem Preset gleich bleibt. */
inline bool lockedByDefault (int i)
{
    const auto& c = controls()[(std::size_t) i];
    return ! c.reserved && c.kind == BUTTON && (c.y < 0.0f && c.x < -60.0f);
}

/** Feste Regler senden auf MIDI-Kanal 1, Preset n auf Kanal n + 1. Das ist die ganze Umschalt-Logik. */
inline int midiChannelFor (int preset, bool locked) { return locked ? 0 : preset + 1; }

/** Das Skript schickt jede Bewegung als Note 70 + Reglernummer, der Anschlag traegt den Wert. */
constexpr int VALUE_NOTE_BASE = 70;
inline float valueFromVelocity (int vel) { return vel <= 1 ? 0.0f : (float) (vel - 1) / 126.0f; }

/** Noten aus dem Piano Roll spielen die Sounds der Tasten: S 1-8 = C1 bis G1, M = G#1 bis D#2, R = E2 bis B2, dann Cycle bis Rec. */
inline int triggerNote (int ctl)
{
    const auto& c = controls()[(std::size_t) ctl];
    if (c.kind != BUTTON || c.reserved) return -1;
    if (c.cc >= 32 && c.cc < 40) return 36 + c.cc - 32;
    if (c.cc >= 48 && c.cc < 56) return 44 + c.cc - 48;
    if (c.cc >= 64 && c.cc < 72) return 52 + c.cc - 64;
    static const int order[9] = { 46, 60, 61, 62, 43, 44, 42, 41, 45 };
    for (int i = 0; i < 9; ++i) if (order[i] == c.cc) return 60 + i;
    return -1;
}
inline int controlForNote (int note)
{
    for (int i = 0; i < NUM_CTL; ++i) if (triggerNote (i) == note) return i;
    return -1;
}

/** Gedruckte Beschriftung auf dem Geraet */
struct Print { const char* text; float x, y, size; bool bold; bool centred; };
inline const std::array<Print, 45>& prints()
{
    static const std::array<Print, 45> p = [] {
        std::array<Print, 45> a {};
        int i = 0;
        a[(std::size_t) i++] = { "JULICAN", -156.0f, 33.0f, 6.8f, true, false };
        a[(std::size_t) i++] = { "nano basics", -156.0f, 25.0f, 4.0f, false, false };
        a[(std::size_t) i++] = { "TRACK", -146.5f, 13.5f, 2.5f, true, false };
        a[(std::size_t) i++] = { "CYCLE", -146.5f, -0.5f, 2.5f, true, false };
        a[(std::size_t) i++] = { "MARKER", -111.5f, -0.5f, 2.5f, true, false };
        a[(std::size_t) i++] = { "SET", -107.5f, -11.0f, 2.5f, true, true };
        a[(std::size_t) i++] = { "<", -93.5f, -11.0f, 2.8f, true, true };
        a[(std::size_t) i++] = { ">", -79.5f, -11.0f, 2.8f, true, true };
        static const char* sym[5] = { "<<", ">>", "STOP", "PLAY", "REC" };
        for (int t = 0; t < 5; ++t) a[(std::size_t) i++] = { sym[t], -139.5f + (float) t * 17.0f, -32.0f, 2.5f, true, true };
        static const char* num[8] = { "1", "2", "3", "4", "5", "6", "7", "8" };
        static const char* smr[3] = { "S", "M", "R" };
        const float x0 = BODY_W * 0.5f - 7.0f - 8.0f * 27.0f;
        for (int ch = 0; ch < 8; ++ch)
        {
            const float cx = x0 + 27.0f * ((float) ch + 0.5f);
            a[(std::size_t) i++] = { num[ch], cx, 36.5f, 2.8f, true, true };
            for (int k = 0; k < 3; ++k) a[(std::size_t) i++] = { smr[k], cx - 11.8f, k == 0 ? 8.0f : k == 1 ? -5.0f : -18.0f, 2.8f, true, true };
        }
        return a;
    }();
    return p;
}
} // namespace nb
