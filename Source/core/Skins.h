// julicans nano basics - die Designs der Oberflaeche. Ohne JUCE, damit sie sich testen lassen.
// Jedes Design ist ein Rauschfeld plus eine Farbtabelle, wie feTurbulence und feComponentTransfer im Mockup.
// Bewegte Designs schieben zwei vorberechnete, kachelbare Felder gegeneinander. Das ist billig genug fuer 30 Bilder pro Sekunde.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace nb
{
struct Skin
{
    const char* key; const char* name; int group;     // 0 Camouflage, 1 Dark Matter, 2 Abstrakt
    bool turbulence; float fx, fy; int octaves; int seed;
    bool discrete; std::vector<uint32_t> cols;
    bool reactive, stars; float speed;
};

enum Group { CAMO = 0, DARK_MATTER, ABSTRACT, NUM_GROUPS };
inline const char* groupName (int g) { static const char* n[NUM_GROUPS] = { "Camouflage", "Dark Matter", "Abstrakt" }; return n[g]; }

inline uint32_t dimCol (uint32_t c, float k)
{
    const auto ch = [&] (int s) { return (uint32_t) std::lround (std::clamp ((float) ((c >> s) & 255u) * k, 0.0f, 255.0f)) << s; };
    return 0xff000000u | ch (16) | ch (8) | ch (0);
}

inline const std::vector<Skin>& skins()
{
    static const std::vector<Skin> s = [] {
        std::vector<Skin> v;
        const auto camo = [&] (const char* k, const char* n, std::array<uint32_t, 4> p, float fx, float fy, int seed)
        { v.push_back ({ k, n, CAMO, false, fx, fy, 3, seed, true, { p[0], p[0], p[0], p[1], p[2], p[3], p[3], p[3] }, false, false, 0 }); };
        const auto dm = [&] (const char* k, const char* n, uint32_t a, uint32_t b, int seed)
        {
            const uint32_t bg = 0xff040207;
            v.push_back ({ k, n, DARK_MATTER, false, 0.009f, 0.017f, 5, seed, false,
                { bg, bg, bg, bg, bg, bg, bg, bg, bg, 0xff060309, 0xff070410, b, 0xff080412, dimCol (a, 0.25f), dimCol (a, 0.55f), a, b, b, b, 0xffffffff },
                true, true, 1.0f });
        };
        const auto neon = [&] (const char* k, const char* n, uint32_t bg, uint32_t line, int seed)
        {
            std::vector<uint32_t> c; for (int i = 0; i < 45; ++i) c.push_back (i % 5 == 4 ? line : bg);
            v.push_back ({ k, n, ABSTRACT, false, 0.006f, 0.013f, 2, seed, false, c, true, false, 1.3f });
        };
        const auto aurora = [&] (const char* k, const char* n, std::array<uint32_t, 4> c, int seed)
        { v.push_back ({ k, n, ABSTRACT, false, 0.004f, 0.012f, 3, seed, false, { c[0], c[0], c[1], c[2], c[3], c[3] }, true, false, 0.8f }); };
        const auto split = [&] (const char* k, const char* n, std::array<uint32_t, 4> c, int seed)
        { v.push_back ({ k, n, ABSTRACT, true, 0.018f, 0.03f, 1, seed, true, { c[0], c[1], c[2], c[3], c[1], c[2], c[0], c[3], c[2] }, false, false, 0 }); };
        const auto chrom = [&] (const char* k, const char* n, uint32_t a, uint32_t b, int seed)
        { v.push_back ({ k, n, ABSTRACT, false, 0.011f, 0.02f, 3, seed, false, { a, b, a, b, a, b, a }, false, false, 0 }); };

        camo ("camo_wald", "Wald", { 0xff1c1f17, 0xff2f3a23, 0xff4b5a2e, 0xff6b5a3a }, 0.016f, 0.03f, 4);
        camo ("camo_wueste", "Wüste", { 0xff7c5e3c, 0xffa07c52, 0xffc2a476, 0xffe0c99a }, 0.016f, 0.03f, 9);
        camo ("camo_urban", "Urban", { 0xff26282b, 0xff4a4d52, 0xff7d8086, 0xffa9abaf }, 0.016f, 0.03f, 21);
        camo ("camo_arktis", "Arktis", { 0xff7f8b95, 0xffb9c2c9, 0xffe8edf0, 0xffffffff }, 0.03f, 0.055f, 5);
        camo ("camo_nacht", "Nacht Neon", { 0xff0b0d10, 0xff1d2229, 0xff2c333b, 0xff00e5ff }, 0.03f, 0.055f, 13);
        camo ("camo_toxic", "Toxic", { 0xff11140e, 0xff2a3320, 0xff3f4a2c, 0xffa6ff00 }, 0.03f, 0.055f, 17);
        dm ("dm_klassisch", "Klassisch", 0xff7a1cff, 0xffff2bd6, 11);
        dm ("dm_blutrot", "Blutrot", 0xffff1a1a, 0xffff8a1a, 12);
        dm ("dm_eisblau", "Eisblau", 0xff0090ff, 0xff00ffe0, 14);
        dm ("dm_gold", "Gold", 0xffffb000, 0xffff5e00, 15);
        dm ("dm_gift", "Gift", 0xff30ff60, 0xffc0ff00, 16);
        neon ("neon_cyan", "Neon-Linien Cyan", 0xff04080c, 0xff00e8ff, 3);
        neon ("neon_pink", "Neon-Linien Pink", 0xff0b0408, 0xffff2e9a, 6);
        neon ("neon_acid", "Neon-Linien Acid", 0xff060a02, 0xffb6ff00, 8);
        aurora ("aurora_nordlicht", "Aurora Nordlicht", { 0xff020611, 0xff00ff9c, 0xff00b3ff, 0xff8a2be2 }, 2);
        aurora ("aurora_lava", "Aurora Lava", { 0xff0a0200, 0xffff2a00, 0xffff8c00, 0xffffe066 }, 7);
        aurora ("aurora_vapor", "Aurora Vapor", { 0xff0d0221, 0xffff00c8, 0xff00f0ff, 0xff7b2cff }, 10);
        split ("splitter_sunset", "Splitter Sunset", { 0xff2b0f3a, 0xffc2185b, 0xffff7043, 0xffffca28 }, 3);
        split ("splitter_ozean", "Splitter Ozean", { 0xff021b2b, 0xff0a4d68, 0xff088395, 0xff05bfdb }, 5);
        split ("splitter_mono", "Splitter Mono", { 0xff0d0d0d, 0xff2a2a2a, 0xff4a4a4a, 0xff7a7a7a }, 8);
        chrom ("chrom_silber", "Chrom Silber", 0xff7d838a, 0xffeef0f2, 4);
        chrom ("chrom_schwarz", "Schwarzchrom", 0xff0c0c0e, 0xff4a4a52, 6);
        chrom ("chrom_rose", "Chrom Rosé", 0xff9e5a64, 0xfff6cfcf, 9);
        return v;
    }();
    return s;
}
inline int skinIndex (const std::string& key)
{
    for (int i = 0; i < (int) skins().size(); ++i) if (key == skins()[(size_t) i].key) return i;
    return 6;
}

// ---------------- Rauschen ---------------------------------------------------
/** Kachelbares Perlin-Rauschen in 2D. Gitter wiederholt sich nach px mal py Zellen. */
class Perlin
{
public:
    explicit Perlin (int seed)
    {
        uint32_t s = (uint32_t) seed * 2654435761u + 12345u;
        for (int i = 0; i < 256; ++i) p[(size_t) i] = (uint8_t) i;
        for (int i = 255; i > 0; --i) { s = s * 1664525u + 1013904223u; std::swap (p[(size_t) i], p[(size_t) ((s >> 8) % (uint32_t) (i + 1))]); }
        for (int i = 0; i < 256; ++i) { const float a = (float) p[(size_t) i] / 256.0f * 6.2831853f; gx[(size_t) i] = std::cos (a); gy[(size_t) i] = std::sin (a); }
    }
    float noise (float x, float y, int px, int py) const
    {
        const int x0 = (int) std::floor (x), y0 = (int) std::floor (y);
        const float fx = x - (float) x0, fy = y - (float) y0;
        const auto grad = [&] (int ix, int iy, float dx, float dy)
        {
            ix = ((ix % px) + px) % px; iy = ((iy % py) + py) % py;
            const int h = p[(size_t) ((p[(size_t) (ix & 255)] + iy) & 255)];
            return gx[(size_t) h] * dx + gy[(size_t) h] * dy;
        };
        const auto fade = [] (float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
        const float u = fade (fx), v = fade (fy);
        const float a = grad (x0, y0, fx, fy), b = grad (x0 + 1, y0, fx - 1, fy);
        const float c = grad (x0, y0 + 1, fx, fy - 1), d = grad (x0 + 1, y0 + 1, fx - 1, fy - 1);
        return (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v;
    }
private:
    std::array<uint8_t, 256> p {}; std::array<float, 256> gx {}, gy {};
};

/** Vorberechnetes Feld in Pufferaufloesung, Werte 0..1, kachelbar in beide Richtungen. */
struct Field
{
    int w = 0, h = 0; std::vector<float> v;
    float at (float x, float y) const
    {
        x -= std::floor (x / (float) w) * (float) w; y -= std::floor (y / (float) h) * (float) h;
        const int x0 = std::min ((int) x, w - 1), y0 = std::min ((int) y, h - 1), x1 = (x0 + 1) % w, y1 = (y0 + 1) % h;
        const float fx = x - (float) x0, fy = y - (float) y0;
        const float a = v[(size_t) (y0 * w + x0)], b = v[(size_t) (y0 * w + x1)], c = v[(size_t) (y1 * w + x0)], d = v[(size_t) (y1 * w + x1)];
        return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
    }
};

/** Das Feld deckt die ganze Oberflaeche (325 x 83 mm) ab, Frequenzen in 1/mm wie im Mockup. */
inline Field makeField (const Skin& s, int w, int h, int seedOffset = 0)
{
    Field f; f.w = w; f.h = h; f.v.assign ((size_t) (w * h), 0.0f);
    const Perlin pn (s.seed + seedOffset * 101);
    for (int o = 0; o < s.octaves; ++o)
    {
        const float m = (float) (1 << o), amp = 1.0f / m;
        const int px = std::max (1, (int) std::lround (s.fx * m * 325.0f)), py = std::max (1, (int) std::lround (s.fy * m * 83.0f));
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
            {
                const float n = pn.noise (((float) i + 0.5f) / (float) w * (float) px, ((float) j + 0.5f) / (float) h * (float) py, px, py);
                f.v[(size_t) (j * w + i)] += (s.turbulence ? std::fabs (n) : n) * amp;
            }
    }
    for (auto& x : f.v) x = std::clamp (s.turbulence ? x : (x + 1.0f) * 0.5f, 0.0f, 1.0f);
    return f;
}

/** Farbtabelle wie feComponentTransfer: diskret in Stufen oder linear zwischen den Eintraegen. */
inline uint32_t mapColour (const Skin& s, float x, float gain = 1.0f)
{
    const int n = (int) s.cols.size();
    x = std::clamp (x, 0.0f, 1.0f);
    float r, g, b;
    const auto rgb = [&] (uint32_t c, float& R, float& G, float& B) { R = (float) ((c >> 16) & 255u); G = (float) ((c >> 8) & 255u); B = (float) (c & 255u); };
    if (s.discrete) rgb (s.cols[(size_t) std::min (n - 1, (int) (x * (float) n))], r, g, b);
    else
    {
        const float f = x * (float) (n - 1); const int a = std::min ((int) f, n - 2); const float t = f - (float) a;
        float r2, g2, b2; rgb (s.cols[(size_t) a], r, g, b); rgb (s.cols[(size_t) a + 1], r2, g2, b2);
        r += (r2 - r) * t; g += (g2 - g) * t; b += (b2 - b) * t;
    }
    const auto c = [&] (float v) { return (uint32_t) std::clamp (v * gain, 0.0f, 255.0f); };
    return 0xff000000u | (c (r) << 16) | (c (g) << 8) | c (b);
}

// ---------------- Bewegung --------------------------------------------------
struct Wave { float x = 0, y = 0, age = 1; };      // x, y in mm (Geraetemitte), age 0..1
struct Motion
{
    float time = 0;        // Sekunden
    float pulse = 0;       // Kick oder Takt, 0..1
    float level = 0;       // Lautstaerke, 0..1
    float flash = 0;       // Preset-Wechsel, 0..1
    std::array<Wave, 8> waves {};
};

/** Rechnet ein Bild der Oberflaeche. out ist ARGB, w mal h, dieselbe Groesse wie die Felder. */
inline void renderSkin (const Skin& s, const Field& a, const Field& b, const Motion& m, uint32_t* out)
{
    const int w = a.w, h = a.h;
    const float pxPerMm = (float) w / 325.0f;
    if (! s.reactive)
    {
        for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) out[j * w + i] = mapColour (s, a.v[(size_t) (j * w + i)]);
        return;
    }
    const float t = m.time * s.speed;
    const float ax = t * 2.2f * pxPerMm, ay = t * 0.7f * pxPerMm, bx = -t * 1.4f * pxPerMm, by = t * 1.1f * pxPerMm;
    const float mixA = 0.62f + 0.12f * std::sin (t * 0.31f), mixB = 1.0f - mixA;
    const float norm = 1.0f / std::sqrt (mixA * mixA + mixB * mixB);
    const float baseGain = 1.0f + 0.15f * m.level + 0.9f * m.pulse + 0.6f * m.flash;
    for (int j = 0; j < h; ++j)
    {
        const float ymm = 41.5f - ((float) j + 0.5f) / pxPerMm;
        for (int i = 0; i < w; ++i)
        {
            const float xmm = ((float) i + 0.5f) / pxPerMm - 162.5f;
            float push = 0, ring = 0;
            for (const auto& wv : m.waves)
            {
                if (wv.age >= 1.0f) continue;
                const float dx = xmm - wv.x, dy = ymm - wv.y, r = std::sqrt (dx * dx + dy * dy);
                const float front = wv.age * 140.0f, d = (r - front) / 7.0f;
                const float k = std::exp (-d * d) * (1.0f - wv.age);
                ring += k; push += k * 3.0f;
            }
            const float x = (float) i + push, y = (float) j - push * 0.5f;
            const float va = a.at (x + ax, y + ay), vb = b.at (x + bx, y + by);
            const float v = 0.5f + ((va * mixA + vb * mixB) - 0.5f) * norm;
            out[j * w + i] = mapColour (s, v + ring * 0.04f, baseGain + ring * 1.4f);
        }
    }
}
} // namespace nb
