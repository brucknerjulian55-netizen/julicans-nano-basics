// Prueft Layout und Designs ohne JUCE. Mit "bilder" als Argument schreibt es eine Vorschau als PPM.
#include "../Source/core/Layout.h"
#include "../Source/core/Skins.h"
#include <cstdio>
#include <set>
#include <string>

using namespace nb;
static int fails = 0;
static void check (bool ok, const char* what) { if (! ok) { ++fails; std::printf ("FEHLER: %s\n", what); } }

int main (int argc, char** argv)
{
    const bool images = argc > 1 && std::string (argv[1]) == "bilder";
    // Layout: jede CC nur einmal, alle Regler auf dem Geraet, Track-Tasten reserviert
    std::set<int> ccs; int reserved = 0, leds = 0;
    for (int i = 0; i < NUM_CTL; ++i)
    {
        const auto& c = controls()[(size_t) i];
        check (ccs.insert (c.cc).second, "CC doppelt");
        check (std::fabs (c.x) + c.w * 0.5f < BODY_W * 0.5f && std::fabs (c.y) + c.h * 0.5f < BODY_H * 0.5f, "Regler ausserhalb");
        check (controlForCc (c.cc) == i, "CC-Zuordnung");
        reserved += c.reserved; leds += c.led;
    }
    check (reserved == 2, "zwei Track-Tasten reserviert");
    std::printf ("%d Regler, %d mit LED, Standard fest: ", NUM_CTL, leds);
    for (int i = 0; i < NUM_CTL; ++i) if (lockedByDefault (i)) std::printf ("%s ", controls()[(size_t) i].id);
    std::printf ("\n");
    check (midiChannelFor (0, true) == 0 && midiChannelFor (0, false) == 1 && midiChannelFor (7, false) == 8, "Kanaele");

    // Designs: jedes nutzt mehrere Farben, bewegte veraendern sich ueber die Zeit, Wellen wirken lokal
    const int W = 520, H = 133;
    std::vector<uint32_t> all;
    for (size_t k = 0; k < skins().size(); ++k)
    {
        const auto& s = skins()[k];
        const auto A = makeField (s, W, H, 0), B = makeField (s, W, H, 1);
        std::vector<uint32_t> img ((size_t) (W * H)), img2 ((size_t) (W * H));
        Motion m; renderSkin (s, A, B, m, img.data());
        std::set<uint32_t> used; for (auto c : img) used.insert (c);
        std::printf ("  %-18s Farben %4zu", s.name, used.size());
        check (used.size() >= 3, s.name);
        if (s.reactive)
        {
            m.time = 1.5f; renderSkin (s, A, B, m, img2.data());
            int diff = 0; for (size_t i = 0; i < img.size(); ++i) diff += img[i] != img2[i];
            std::printf ("  bewegt %4.1f %%", 100.0 * diff / (double) img.size());
            check (diff > (int) img.size() / 20, "bewegt sich");
        }
        // Anteil heller Pixel: Dark Matter soll ueberwiegend dunkel sein
        int bright = 0; for (auto c : img) bright += (((c >> 16) & 255) + ((c >> 8) & 255) + (c & 255)) > 150;
        std::printf ("  hell %4.1f %%\n", 100.0 * bright / (double) img.size());
        if (s.group == DARK_MATTER) check (bright < (int) img.size() * 45 / 100, "Dark Matter zu hell");
        all.insert (all.end(), img.begin(), img.end());
        if (k == 6 && images)
        {
            Motion w; w.time = 2.0f; w.pulse = 0.8f; w.waves[0] = { 40.0f, 10.0f, 0.25f };
            renderSkin (s, A, B, w, img2.data());
            all.insert (all.end(), img2.begin(), img2.end());
        }
    }
    if (images)
    {
        FILE* f = std::fopen ("skins.ppm", "wb");
        const int rows = (int) (all.size() / (size_t) W);
        std::fprintf (f, "P6 %d %d 255\n", W, rows);
        for (auto c : all) { const unsigned char p[3] = { (unsigned char) (c >> 16), (unsigned char) (c >> 8), (unsigned char) c }; std::fwrite (p, 1, 3, f); }
        std::fclose (f);
    }
    std::printf (fails == 0 ? "alles ok\n" : "%d Fehler\n", fails);
    return fails == 0 ? 0 : 1;
}
