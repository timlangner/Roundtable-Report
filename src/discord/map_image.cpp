#include "discord/map_image.hpp"

#include "game/location.hpp"
#include "util.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace erstats {
namespace {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

struct Point {
    float x = 0;
    float y = 0;
};

constexpr int k_width = 560;
constexpr int k_height = 400;
constexpr int k_photo_width = 800;
constexpr int k_photo_height = 500;
constexpr float k_photo_zoom = 0.30f;
constexpr int k_atlas_max_edge = 4096;

constexpr Color k_parchment{18, 14, 10};
constexpr Color k_panel{28, 22, 16};
constexpr Color k_land{58, 50, 34};
constexpr Color k_land_hi{74, 64, 42};
constexpr Color k_water{22, 30, 36};
constexpr Color k_caelid{74, 40, 28};
constexpr Color k_gold{195, 164, 86};
constexpr Color k_gold_dim{120, 98, 52};
constexpr Color k_text{212, 196, 154};
constexpr Color k_pin_core{139, 30, 30};
constexpr Color k_white{236, 228, 210};

constexpr Color k_shadow_bg{16, 12, 20};
constexpr Color k_shadow_land{42, 34, 40};
constexpr Color k_shadow_gold{168, 132, 86};

constexpr Color k_under_bg{10, 14, 20};
constexpr Color k_under_land{26, 40, 56};
constexpr Color k_under_gold{90, 138, 154};

// 5x7 glyphs, bit 0 is the leftmost pixel of each row.
constexpr uint8_t k_font[48][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},  // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},  // B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},  // C
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},  // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},  // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},  // F
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},  // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},  // H
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},  // I
    {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C},  // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},  // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},  // L
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},  // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},  // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},  // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},  // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},  // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},  // R
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E},  // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},  // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},  // U
    {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04},  // V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},  // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},  // X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},  // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},  // Z
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},  // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  // 1
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F},  // 2
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},  // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},  // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},  // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},  // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},  // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},  // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},  // 9
};

int glyph_index(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a';
    }
    if (c >= '0' && c <= '9') {
        return 26 + (c - '0');
    }
    return -1;
}

class Canvas {
public:
    Canvas(int width = k_width, int height = k_height)
        : w_(width), h_(height), pixels_(static_cast<size_t>(width * height), k_parchment) {}

    int width() const { return w_; }
    int height() const { return h_; }

    void fill(Color color) {
        std::fill(pixels_.begin(), pixels_.end(), color);
    }

    void set(int x, int y, Color color) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) {
            return;
        }
        pixels_[static_cast<size_t>(y * w_ + x)] = color;
    }

    void fill_rect(int x, int y, int w, int h, Color color) {
        for (int yy = y; yy < y + h; ++yy) {
            for (int xx = x; xx < x + w; ++xx) {
                set(xx, yy, color);
            }
        }
    }

    void fill_circle(int cx, int cy, int radius, Color color) {
        const int r2 = radius * radius;
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x * x + y * y <= r2) {
                    set(cx + x, cy + y, color);
                }
            }
        }
    }

    void stroke_circle(int cx, int cy, int radius, Color color) {
        const int r2 = radius * radius;
        const int inner = (radius - 1) * (radius - 1);
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                const int d = x * x + y * y;
                if (d <= r2 && d >= inner) {
                    set(cx + x, cy + y, color);
                }
            }
        }
    }

    void line(int x0, int y0, int x1, int y1, Color color) {
        int dx = std::abs(x1 - x0);
        int sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            set(x0, y0, color);
            if (x0 == x1 && y0 == y1) {
                break;
            }
            const int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    void fill_poly(const std::vector<Point>& pts, Color color) {
        if (pts.size() < 3) {
            return;
        }
        float min_y = pts[0].y;
        float max_y = pts[0].y;
        for (const auto& p : pts) {
            min_y = std::min(min_y, p.y);
            max_y = std::max(max_y, p.y);
        }
        const int y0 = std::max(0, static_cast<int>(std::floor(min_y)));
        const int y1 = std::min(h_ - 1, static_cast<int>(std::ceil(max_y)));
        for (int y = y0; y <= y1; ++y) {
            std::vector<float> nodes;
            const float scan = static_cast<float>(y) + 0.5f;
            for (size_t i = 0, n = pts.size(); i < n; ++i) {
                const auto& a = pts[i];
                const auto& b = pts[(i + 1) % n];
                if ((a.y <= scan && b.y > scan) || (b.y <= scan && a.y > scan)) {
                    const float t = (scan - a.y) / (b.y - a.y);
                    nodes.push_back(a.x + t * (b.x - a.x));
                }
            }
            std::sort(nodes.begin(), nodes.end());
            for (size_t i = 0; i + 1 < nodes.size(); i += 2) {
                const int x0 = std::max(0, static_cast<int>(std::floor(nodes[i])));
                const int x1 = std::min(w_ - 1, static_cast<int>(std::ceil(nodes[i + 1])));
                for (int x = x0; x <= x1; ++x) {
                    set(x, y, color);
                }
            }
        }
    }

    void stroke_poly(const std::vector<Point>& pts, Color color) {
        if (pts.size() < 2) {
            return;
        }
        for (size_t i = 0, n = pts.size(); i < n; ++i) {
            const auto& a = pts[i];
            const auto& b = pts[(i + 1) % n];
            line(static_cast<int>(a.x), static_cast<int>(a.y), static_cast<int>(b.x),
                 static_cast<int>(b.y), color);
        }
    }

    int text_width(std::string_view text, int scale) const {
        int w = 0;
        for (char c : text) {
            if (c == ' ' || c == ',' || c == '.' || c == '\'' || c == '-' || c == '+') {
                w += 4 * scale;
            } else if (glyph_index(c) >= 0) {
                w += 6 * scale;
            }
        }
        return w;
    }

    void text(int x, int y, std::string_view value, Color color, int scale = 1) {
        int cursor = x;
        for (char c : value) {
            if (c == ' ') {
                cursor += 4 * scale;
                continue;
            }
            if (c == ',' || c == '.' || c == '\'' || c == '-' || c == '+') {
                if (c == '.' || c == ',') {
                    fill_rect(cursor, y + 5 * scale, scale, scale, color);
                } else if (c == '-') {
                    fill_rect(cursor, y + 3 * scale, 3 * scale, scale, color);
                } else if (c == '+') {
                    fill_rect(cursor + scale, y + 2 * scale, scale, 3 * scale, color);
                    fill_rect(cursor, y + 3 * scale, 3 * scale, scale, color);
                } else {
                    fill_rect(cursor + scale, y, scale, 3 * scale, color);
                }
                cursor += 4 * scale;
                continue;
            }
            const int gi = glyph_index(c);
            if (gi < 0) {
                continue;
            }
            for (int row = 0; row < 7; ++row) {
                const uint8_t bits = k_font[gi][row];
                for (int col = 0; col < 5; ++col) {
                    if (bits & (1u << (4 - col))) {
                        fill_rect(cursor + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
            cursor += 6 * scale;
        }
    }

    void text_centered(int cx, int y, std::string_view value, Color color, int scale = 1) {
        text(cx - text_width(value, scale) / 2, y, value, color, scale);
    }

    std::vector<uint8_t> to_png() const {
        std::vector<uint8_t> raw(static_cast<size_t>((w_ * 4 + 1) * h_));
        size_t i = 0;
        for (int y = 0; y < h_; ++y) {
            raw[i++] = 0;
            for (int x = 0; x < w_; ++x) {
                const auto& p = pixels_[static_cast<size_t>(y * w_ + x)];
                raw[i++] = p.r;
                raw[i++] = p.g;
                raw[i++] = p.b;
                raw[i++] = p.a;
            }
        }
        return encode_png(w_, h_, raw);
    }

private:
    static uint32_t crc32(const uint8_t* data, size_t len) {
        uint32_t c = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            c ^= data[i];
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
        }
        return c ^ 0xFFFFFFFFu;
    }

    static uint32_t adler32(const uint8_t* data, size_t len) {
        uint32_t a = 1;
        uint32_t b = 0;
        for (size_t i = 0; i < len; ++i) {
            a = (a + data[i]) % 65521u;
            b = (b + a) % 65521u;
        }
        return (b << 16) | a;
    }

    static void write_be32(std::vector<uint8_t>& out, uint32_t value) {
        out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    static void write_chunk(
        std::vector<uint8_t>& out, const char type[4], const uint8_t* data, size_t len) {
        write_be32(out, static_cast<uint32_t>(len));
        const size_t start = out.size();
        out.insert(out.end(), type, type + 4);
        out.insert(out.end(), data, data + len);
        write_be32(out, crc32(out.data() + start, 4 + len));
    }

    static std::vector<uint8_t> encode_png(int width, int height, const std::vector<uint8_t>& raw) {
        std::vector<uint8_t> png = {137, 80, 78, 71, 13, 10, 26, 10};
        uint8_t ihdr[13]{};
        ihdr[0] = (width >> 24) & 0xFF;
        ihdr[1] = (width >> 16) & 0xFF;
        ihdr[2] = (width >> 8) & 0xFF;
        ihdr[3] = width & 0xFF;
        ihdr[4] = (height >> 24) & 0xFF;
        ihdr[5] = (height >> 16) & 0xFF;
        ihdr[6] = (height >> 8) & 0xFF;
        ihdr[7] = height & 0xFF;
        ihdr[8] = 8;
        ihdr[9] = 6;
        write_chunk(png, "IHDR", ihdr, sizeof(ihdr));

        std::vector<uint8_t> zlib;
        zlib.push_back(0x78);
        zlib.push_back(0x01);
        size_t offset = 0;
        while (offset < raw.size()) {
            const size_t chunk = std::min<size_t>(65535, raw.size() - offset);
            const bool last = offset + chunk >= raw.size();
            zlib.push_back(last ? 0x01 : 0x00);
            zlib.push_back(static_cast<uint8_t>(chunk & 0xFF));
            zlib.push_back(static_cast<uint8_t>((chunk >> 8) & 0xFF));
            zlib.push_back(static_cast<uint8_t>((~chunk) & 0xFF));
            zlib.push_back(static_cast<uint8_t>(((~chunk) >> 8) & 0xFF));
            zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                        raw.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
            offset += chunk;
        }
        write_be32(zlib, adler32(raw.data(), raw.size()));
        write_chunk(png, "IDAT", zlib.data(), zlib.size());
        write_chunk(png, "IEND", nullptr, 0);
        return png;
    }

    int w_ = k_width;
    int h_ = k_height;
    std::vector<Color> pixels_;
};

Point px(float u, float v) {
    return {40.f + u * 480.f, 48.f + v * 316.f};
}

std::vector<Point> lands_between() {
    const Point n[] = {
        px(0.38f, 0.92f), px(0.55f, 0.94f), px(0.62f, 0.86f), px(0.68f, 0.78f),
        px(0.72f, 0.70f), px(0.82f, 0.68f), px(0.90f, 0.62f), px(0.92f, 0.52f),
        px(0.88f, 0.42f), px(0.84f, 0.34f), px(0.80f, 0.24f), px(0.74f, 0.14f),
        px(0.62f, 0.08f), px(0.52f, 0.06f), px(0.46f, 0.10f), px(0.42f, 0.18f),
        px(0.48f, 0.24f), px(0.52f, 0.30f), px(0.38f, 0.28f), px(0.28f, 0.32f),
        px(0.18f, 0.38f), px(0.14f, 0.48f), px(0.16f, 0.58f), px(0.22f, 0.62f),
        px(0.28f, 0.70f), px(0.36f, 0.78f), px(0.38f, 0.88f),
    };
    return {std::begin(n), std::end(n)};
}

void draw_frame(Canvas& c, Color bg, Color gold) {
    c.fill(bg);
    c.fill_rect(12, 12, k_width - 24, k_height - 24, k_panel);
    c.fill_rect(14, 14, k_width - 28, 2, gold);
    c.fill_rect(14, k_height - 16, k_width - 28, 2, gold);
    c.fill_rect(14, 14, 2, k_height - 28, gold);
    c.fill_rect(k_width - 16, 14, 2, k_height - 28, gold);
}

void draw_pin_at(Canvas& c, int x, int y, std::string_view label, Color gold) {
    c.line(x - 14, y, x + 14, y, gold);
    c.line(x, y - 14, x, y + 14, gold);
    c.fill_circle(x, y, 8, gold);
    c.fill_circle(x, y, 5, k_pin_core);
    c.fill_circle(x, y, 2, k_white);
    if (!label.empty()) {
        c.text_centered(x, y + 16, label, k_text, 1);
    }
}

void draw_lands_between(Canvas& c, const MapPin& pin, std::string_view location) {
    draw_frame(c, k_parchment, k_gold);
    c.text_centered(k_width / 2, 22, "THE LANDS BETWEEN", k_gold, 2);
    c.fill_poly(lands_between(), k_land);
    c.fill_poly({px(0.26f, 0.48f), px(0.40f, 0.46f), px(0.42f, 0.56f), px(0.30f, 0.60f)}, k_water);
    c.fill_poly({px(0.70f, 0.52f), px(0.90f, 0.50f), px(0.88f, 0.64f), px(0.72f, 0.68f)}, k_caelid);
    c.stroke_poly(lands_between(), k_gold_dim);
    c.text(static_cast<int>(px(0.46f, 0.72f).x), static_cast<int>(px(0.46f, 0.72f).y), "LIMGRAVE",
           k_gold_dim, 1);
    c.text(static_cast<int>(px(0.24f, 0.50f).x), static_cast<int>(px(0.24f, 0.50f).y), "LIURNIA",
           k_gold_dim, 1);
    c.text(static_cast<int>(px(0.36f, 0.32f).x), static_cast<int>(px(0.36f, 0.32f).y), "ALTUS",
           k_gold_dim, 1);
    c.text(static_cast<int>(px(0.70f, 0.58f).x), static_cast<int>(px(0.70f, 0.58f).y), "CAELID",
           k_gold_dim, 1);
    c.text(static_cast<int>(px(0.58f, 0.16f).x), static_cast<int>(px(0.58f, 0.16f).y), "MOUNTAINS",
           k_gold_dim, 1);
    const auto p = px(pin.u, pin.v);
    draw_pin_at(c, static_cast<int>(p.x), static_cast<int>(p.y), location, k_gold);
}

void draw_shadow(Canvas& c, const MapPin& pin, std::string_view location) {
    draw_frame(c, k_shadow_bg, k_shadow_gold);
    c.text_centered(k_width / 2, 22, "THE REALM OF SHADOW", k_shadow_gold, 2);
    c.fill_poly({px(0.18f, 0.78f), px(0.42f, 0.82f), px(0.70f, 0.74f), px(0.86f, 0.58f),
                 px(0.80f, 0.28f), px(0.52f, 0.16f), px(0.24f, 0.28f), px(0.14f, 0.52f)},
                k_shadow_land);
    c.stroke_poly({px(0.18f, 0.78f), px(0.42f, 0.82f), px(0.70f, 0.74f), px(0.86f, 0.58f),
                   px(0.80f, 0.28f), px(0.52f, 0.16f), px(0.24f, 0.28f), px(0.14f, 0.52f)},
                  k_shadow_gold);
    c.text(static_cast<int>(px(0.22f, 0.56f).x), static_cast<int>(px(0.22f, 0.56f).y), "GRAVESITE",
           k_shadow_gold, 1);
    c.text(static_cast<int>(px(0.48f, 0.34f).x), static_cast<int>(px(0.48f, 0.34f).y), "SCADU",
           k_shadow_gold, 1);
    c.text(static_cast<int>(px(0.68f, 0.52f).x), static_cast<int>(px(0.68f, 0.52f).y), "WOODS",
           k_shadow_gold, 1);
    const auto p = px(pin.u, pin.v);
    draw_pin_at(c, static_cast<int>(p.x), static_cast<int>(p.y), location, k_shadow_gold);
}

void draw_underground(Canvas& c, const MapPin& pin, std::string_view location) {
    draw_frame(c, k_under_bg, k_under_gold);
    c.text_centered(k_width / 2, 22, "THE UNDERGROUND", k_under_gold, 2);
    c.fill_poly({px(0.46f, 0.72f), px(0.70f, 0.60f), px(0.68f, 0.38f), px(0.50f, 0.30f),
                 px(0.36f, 0.42f), px(0.38f, 0.62f)},
                k_under_land);
    c.fill_poly({px(0.20f, 0.58f), px(0.38f, 0.50f), px(0.36f, 0.36f), px(0.18f, 0.40f)},
                k_under_land);
    c.stroke_poly({px(0.46f, 0.72f), px(0.70f, 0.60f), px(0.68f, 0.38f), px(0.50f, 0.30f),
                   px(0.36f, 0.42f), px(0.38f, 0.62f)},
                  k_under_gold);
    c.text(static_cast<int>(px(0.50f, 0.56f).x), static_cast<int>(px(0.50f, 0.56f).y), "SIOFRA",
           k_under_gold, 1);
    c.text(static_cast<int>(px(0.18f, 0.46f).x), static_cast<int>(px(0.18f, 0.46f).y), "AINSEL",
           k_under_gold, 1);
    const auto p = px(pin.u, pin.v);
    draw_pin_at(c, static_cast<int>(p.x), static_cast<int>(p.y), location, k_under_gold);
}

struct AtlasImage {
    int w = 0;
    int h = 0;
    std::vector<Color> pixels;
};

std::vector<const char*> atlas_stems(MapAtlas atlas) {
    switch (atlas) {
        case MapAtlas::Shadow:
            return {"realm_of_shadow", "shadow", "dlc"};
        case MapAtlas::Underground:
            return {"underworld", "underground"};
        case MapAtlas::LandsBetween:
        default:
            return {"lands_between", "overworld"};
    }
}

// Pin UVs are calibrated directly in full-image space (see location.cpp), so
// no extra margins are applied here; only the frame color differs per atlas.
struct AtlasCalib {
    float u0 = 0.f;
    float v0 = 0.f;
    float u1 = 1.f;
    float v1 = 1.f;
    Color frame = k_gold;
};

AtlasCalib atlas_calib(MapAtlas atlas) {
    switch (atlas) {
        case MapAtlas::Shadow:
            return {0.f, 0.f, 1.f, 1.f, k_shadow_gold};
        case MapAtlas::Underground:
            return {0.f, 0.f, 1.f, 1.f, k_under_gold};
        case MapAtlas::LandsBetween:
        default:
            return {0.f, 0.f, 1.f, 1.f, k_gold};
    }
}

std::vector<std::filesystem::path> atlas_search_roots() {
    std::vector<std::filesystem::path> roots;
    const auto dll = dll_directory();
    if (!dll.empty()) {
        roots.push_back(dll / "maps");
        roots.push_back(dll);
    }
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        roots.push_back(cwd / "maps");
        roots.push_back(cwd / "assets" / "maps");
        roots.push_back(cwd.parent_path() / "assets" / "maps");
        roots.push_back(cwd.parent_path().parent_path() / "assets" / "maps");
    }
    return roots;
}

std::optional<std::filesystem::path> find_atlas_file(MapAtlas atlas) {
    const char* exts[] = {".png", ".jpg", ".jpeg"};
    for (const auto& root : atlas_search_roots()) {
        std::filesystem::path best;
        std::uintmax_t best_size = 0;
        for (const char* stem : atlas_stems(atlas)) {
            for (const char* ext : exts) {
                const auto path = root / (std::string(stem) + ext);
                std::error_code ec;
                if (!std::filesystem::exists(path, ec)) {
                    continue;
                }
                const auto size = std::filesystem::file_size(path, ec);
                if (!ec && size > best_size) {
                    best = path;
                    best_size = size;
                }
            }
        }
        if (!best.empty()) {
            return best;
        }
    }
    return std::nullopt;
}

Color sample_bilinear(const AtlasImage& image, float x, float y);

AtlasImage downsample_atlas(const AtlasImage& src, int max_edge) {
    const int edge = (std::max)(src.w, src.h);
    if (edge <= max_edge) {
        return src;
    }
    const float scale = static_cast<float>(max_edge) / static_cast<float>(edge);
    AtlasImage out;
    out.w = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(src.w) * scale)));
    out.h = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(src.h) * scale)));
    out.pixels.resize(static_cast<size_t>(out.w * out.h));
    for (int y = 0; y < out.h; ++y) {
        for (int x = 0; x < out.w; ++x) {
            const float sx = (static_cast<float>(x) + 0.5f) * src.w / out.w;
            const float sy = (static_cast<float>(y) + 0.5f) * src.h / out.h;
            out.pixels[static_cast<size_t>(y * out.w + x)] = sample_bilinear(src, sx, sy);
        }
    }
    return out;
}

std::optional<AtlasImage> decode_atlas_file(const std::filesystem::path& path) {
    const auto bytes = read_binary_file(path);
    if (bytes.empty()) {
        return std::nullopt;
    }
    int w = 0;
    int h = 0;
    int comp = 0;
    unsigned char* data = stbi_load_from_memory(
        bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, 4);
    if (data == nullptr || w <= 0 || h <= 0) {
        return std::nullopt;
    }
    AtlasImage image;
    image.w = w;
    image.h = h;
    image.pixels.resize(static_cast<size_t>(w * h));
    for (int i = 0; i < w * h; ++i) {
        image.pixels[static_cast<size_t>(i)] = Color{
            data[i * 4 + 0], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]};
    }
    stbi_image_free(data);
    if ((std::max)(image.w, image.h) > k_atlas_max_edge) {
        log_info(
            "loaded map " + path.filename().string() + " " + std::to_string(image.w) + "x"
            + std::to_string(image.h) + ", downsampling for Discord");
        image = downsample_atlas(image, k_atlas_max_edge);
    } else {
        log_info(
            "loaded map " + path.filename().string() + " " + std::to_string(image.w) + "x"
            + std::to_string(image.h));
    }
    return image;
}

std::optional<AtlasImage> load_atlas_image(MapAtlas atlas) {
    const auto path = find_atlas_file(atlas);
    if (!path) {
        return std::nullopt;
    }
    static AtlasImage cache[3];
    static std::filesystem::path cache_path[3];
    const int index = atlas == MapAtlas::Shadow ? 1 : atlas == MapAtlas::Underground ? 2 : 0;
    if (cache_path[index] == *path && !cache[index].pixels.empty()) {
        return cache[index];
    }
    auto image = decode_atlas_file(*path);
    if (!image) {
        return std::nullopt;
    }
    cache[index] = *image;
    cache_path[index] = *path;
    return image;
}

Color sample_bilinear(const AtlasImage& image, float x, float y) {
    x = std::max(0.f, std::min(static_cast<float>(image.w - 1), x));
    y = std::max(0.f, std::min(static_cast<float>(image.h - 1), y));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(image.w - 1, x0 + 1);
    const int y1 = std::min(image.h - 1, y0 + 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const auto at = [&](int px, int py) -> Color {
        return image.pixels[static_cast<size_t>(py * image.w + px)];
    };
    const auto mix = [](uint8_t a, uint8_t b, float t) {
        return static_cast<uint8_t>(std::lround(static_cast<float>(a) * (1.f - t) + static_cast<float>(b) * t));
    };
    const Color c00 = at(x0, y0);
    const Color c10 = at(x1, y0);
    const Color c01 = at(x0, y1);
    const Color c11 = at(x1, y1);
    return Color{
        mix(mix(c00.r, c10.r, tx), mix(c01.r, c11.r, tx), ty),
        mix(mix(c00.g, c10.g, tx), mix(c01.g, c11.g, tx), ty),
        mix(mix(c00.b, c10.b, tx), mix(c01.b, c11.b, tx), ty),
        255,
    };
}

bool draw_photo_map(Canvas& canvas, const AtlasImage& image, const MapPin& pin, std::string_view location) {
    const auto calib = atlas_calib(pin.atlas);
    const float src_x = (calib.u0 + pin.u * (calib.u1 - calib.u0)) * static_cast<float>(image.w);
    const float src_y = (calib.v0 + pin.v * (calib.v1 - calib.v0)) * static_cast<float>(image.h);

    const int out_w = canvas.width();
    const int out_h = canvas.height();
    int crop_w = std::max(64, static_cast<int>(std::lround(static_cast<float>(image.w) * k_photo_zoom)));
    int crop_h = std::max(64, static_cast<int>(std::lround(static_cast<float>(crop_w) * out_h / out_w)));
    crop_w = std::min(crop_w, image.w);
    crop_h = std::min(crop_h, image.h);
    int x0 = static_cast<int>(std::lround(src_x - crop_w * 0.5f));
    int y0 = static_cast<int>(std::lround(src_y - crop_h * 0.5f));
    x0 = std::max(0, std::min(image.w - crop_w, x0));
    y0 = std::max(0, std::min(image.h - crop_h, y0));

    for (int y = 0; y < out_h; ++y) {
        for (int x = 0; x < out_w; ++x) {
            const float sx = static_cast<float>(x0) + (static_cast<float>(x) + 0.5f) * crop_w / out_w;
            const float sy = static_cast<float>(y0) + (static_cast<float>(y) + 0.5f) * crop_h / out_h;
            canvas.set(x, y, sample_bilinear(image, sx, sy));
        }
    }

    canvas.fill_rect(0, 0, out_w, 3, calib.frame);
    canvas.fill_rect(0, out_h - 3, out_w, 3, calib.frame);
    canvas.fill_rect(0, 0, 3, out_h, calib.frame);
    canvas.fill_rect(out_w - 3, 0, 3, out_h, calib.frame);

    const int pin_x = static_cast<int>(std::lround(
        (src_x - static_cast<float>(x0)) / static_cast<float>(crop_w) * out_w));
    const int pin_y = static_cast<int>(std::lround(
        (src_y - static_cast<float>(y0)) / static_cast<float>(crop_h) * out_h));
    draw_pin_at(canvas, pin_x, pin_y, location, calib.frame);
    return true;
}

}  // namespace

std::optional<std::vector<uint8_t>> render_location_map(const RunSnapshot& snapshot) {
    // Without a real position, pass NaN so pin_from_world falls back to the
    // named-location pin instead of projecting a bogus (0,0) coordinate.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float x = snapshot.live.has_position ? snapshot.live.x : nan;
    const float y = snapshot.live.has_position ? snapshot.live.y : nan;
    const float z = snapshot.live.has_position ? snapshot.live.z : nan;
    const auto pin = pin_from_world(
        snapshot.live.map_id, x, y, z, snapshot.live.location);
    if (!pin.valid) {
        return std::nullopt;
    }

    if (const auto photo = load_atlas_image(pin.atlas)) {
        Canvas canvas(k_photo_width, k_photo_height);
        draw_photo_map(canvas, *photo, pin, snapshot.live.location);
        return canvas.to_png();
    }

    Canvas canvas;
    switch (pin.atlas) {
        case MapAtlas::Shadow:
            draw_shadow(canvas, pin, snapshot.live.location);
            break;
        case MapAtlas::Underground:
            draw_underground(canvas, pin, snapshot.live.location);
            break;
        case MapAtlas::LandsBetween:
        default:
            draw_lands_between(canvas, pin, snapshot.live.location);
            break;
    }
    return canvas.to_png();
}

}  // namespace erstats
