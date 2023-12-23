#include <algorithm>
#include <cassert>
#include <vector>
namespace WDFramer{
class Framer {
    //3x3 column major 2D affine transformation matrix
    struct matrix_t {
        float a,b,c,d,tx,ty;

        static matrix_t identity() {
            return matrix_t{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
        }

        static matrix_t translate(float tx, float ty) {
            return matrix_t{1.0f, 0.0f, 0.0f, 1.0f, tx, ty};
        }

        static matrix_t scale(float sx, float sy) {
            return matrix_t{sx, 0.0f, 0.0f, sy, 0.0f, 0.0f};
        }

        matrix_t() = default;

        matrix_t(float a, float b, float c, float d, float tx, float ty) 
        : a{a},b{b},c{c},d{d},tx{tx},ty{ty} {}

        //3x3 matrix multiplication
        inline matrix_t operator*(const matrix_t& rhs) {
            matrix_t ret;
            ret.a = a * rhs.a + b * rhs.c;
            ret.b = a * rhs.b + b * rhs.d;
            ret.c = c * rhs.a + d * rhs.c;
            ret.d = c * rhs.b + d * rhs.d;
            ret.tx = tx * rhs.a + ty * rhs.c + rhs.tx;
            ret.ty = tx * rhs.b + ty * rhs.d + rhs.ty;
            return ret;
        }

        inline matrix_t concat(const matrix_t& m) {
            return *this * m;
        }
    };

    union vec2_t {
        struct {
            float x, y;
        };
        struct {
            float w, h;
        };
        float d[2];

        inline vec2_t operator+(const vec2_t& rhs) {
            return {this->x + rhs.x, this->y + rhs.y};
        }

        inline vec2_t operator-(const vec2_t& rhs) {
            return {this->x - rhs.x, this->y - rhs.y};
        }

        inline vec2_t operator*(float s) {
            return {this->x * s, this->y * s};
        }

        inline vec2_t apply(const matrix_t& m) {
            return vec2_t{
                x * m.a + y * m.c + m.tx,
                x * m.b + y * m.d + m.ty
            };
        }
    };

    float calculateRatio() {
        float rw = fs.w / ps.w;
        float rh = fs.h / ps.h;
        switch (m) {
            case mode_t::aspectFit:
                return std::min(rw, rh);
            case mode_t::aspectFill:
                return std::max(rw, rh);
            default:
                break;
        }
        return 1.0f;
    }


public:
    enum class mode_t {
        aspectFit,
        aspectFill
    };

    enum class rot_t {
        none,
        cw_90,//90 clockwise
        cw_180,//180 clockwise
        cw_270//270 clockwise
    };

    enum class origin_t {
        center,//x-right, y-up
        bottom_left, //x-right, y-up
        top_left //x-right, y-down
    };

    enum class norm_t {
        none,//no normalization
        normalize, //[0,1]
        NDC //OpenGL NDC space [-1,1]
    };

    enum class system_t {
        picture,
        frame
    };

    Framer(
        vec2_t pictureSize,
        vec2_t frameSize,
        origin_t origin,
        mode_t contentMode
    ) : ps{pictureSize}, 
        fs{frameSize}, 
        orig{origin},
        m{contentMode},
        r{calculateRatio()} {}

    matrix_t rotation(rot_t rot) {
        switch(rot) {
            case rot_t::cw_90:
                return matrix_t{0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f};
            case rot_t::cw_180:
                return matrix_t{-1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f};
            case rot_t::cw_270:
                return matrix_t{0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
            case rot_t::none:
            default:
                return matrix_t::identity();
        }
    }

    matrix_t transfromFrom(system_t from, system_t to) {
        if (from == to) return matrix_t::identity();

        bool isPic2frame = (from == system_t::picture);
        float s = isPic2frame ? r : 1.0f/r;
        switch (orig)
        {
            case origin_t::center:
                return matrix_t::scale(s, s);
            case origin_t::bottom_left:
            case origin_t::top_left: {
                vec2_t f1 = isPic2frame ? ps : fs;
                vec2_t f2 = isPic2frame ? fs : ps;
                auto trans = matrix_t::translate(-f1.w / 2.0f, -f1.h / 2.0f)
                    .concat(matrix_t::scale(s, s))
                    .concat(matrix_t::translate(f2.w / 2.0f, f2.h / 2.0f));
                return trans;
            }
            break;
            default:
                assert(false);
                break;
        }
        return matrix_t::identity();
    }

    void transformPointsFrom(system_t from, system_t to, std::vector<vec2_t>& points, norm_t norm = norm_t::none) {
        if (from == to) return;
        bool isPic2frame = (from == system_t::picture);
        auto trans = transfromFrom(from, to);
        for (auto& p : points) {
            auto ret = p.apply(trans);
            auto norm_size = isPic2frame ? fs : ps;
            switch (norm) {
                case norm_t::none:
                    break;
                case norm_t::normalize:
                    ret = vec2_t{
                        ret.x / norm_size.w,
                        ret.y / norm_size.h
                    };
                    break;
                case norm_t::NDC:
                    if (orig == origin_t::center) {
                        ret = vec2_t{
                            ret.x/(norm_size.w/2),
                            ret.y/(norm_size.h/2)
                        };
                    }
                    else {
                        ret = vec2_t{
                            2.0f * (ret.x / norm_size.w) - 1.0f,
                            2.0f * (ret.y / norm_size.h) - 1.0f
                        };
                    }
                    break;
                default:
                    assert(false);
            }
            p = ret;
        }
    }

    inline vec2_t pic2Frame(vec2_t pic_point, norm_t norm = norm_t::none) {
        std::vector<vec2_t> v = {pic_point};
        transformPointsFrom(system_t::picture, system_t::frame, v, norm);
        return v[0];
    }

    inline vec2_t frame2Pic(vec2_t frame_point, norm_t norm = norm_t::none) {
        std::vector<vec2_t> v = {frame_point};
        transformPointsFrom(system_t::frame, system_t::picture, v, norm);
        return v[0];
    }

    void gl_dynamic_quad(float verts[8], float tex[8], rot_t rot=rot_t::none, bool mirror=false) {
        //order: top-left, top-right, bottom-left, bottom-right
        float t[] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f
        };
        std::vector<vec2_t> v = {
            {0.0f, ps.h},
            {ps.w, ps.h},
            {0.0f, 0.0f},
            {ps.w, 0.0f}
        };

        transformPointsFrom(system_t::picture, system_t::frame, v, norm_t::NDC);

        if (rot != rot_t::none) {
            auto rot_trans = rotation(rot);
            for (auto& p : v) {
                p = p.apply(rot_trans);
            }
        }

        if (mirror) {
            std::swap(v[0], v[1]);
            std::swap(v[2], v[3]);
        }

        memcpy(verts, v.data(), 4*2*4);
        memcpy(tex, t, 4*2*4);
    }

    void gl_fullscreen_quad(float verts[8], float tex[8], rot_t rot=rot_t::none, bool mirror=false) {
        //order: top-left, top-right, bottom-left, bottom-right
        float v[] = {
            -1.0f, 1.0f,
            1.0f, 1.0f,
            -1.0f, -1.0f,
            1.0f, -1.0f
        };

        std::vector<vec2_t> t = {
            {0.0f, fs.h},
            {fs.w, fs.h},
            {0.0f, 0.0f},
            {fs.w, 0.0f}
        };

        transformPointsFrom(system_t::frame, system_t::picture, t, norm_t::normalize);

        if (rot != rot_t::none) {
            auto rot_trans = matrix_t::translate(-0.5f, -0.5f)
                .concat(rotation(rot))
                .concat(matrix_t::translate(0.5f, 0.5f));
            for (auto& p : t) {
                p = p.apply(rot_trans);
            }
        }

        if (mirror) {
            std::swap(t[0], t[1]);
            std::swap(t[2], t[3]);
        }

        memcpy(verts, v, 4*2*4);
        memcpy(tex, t.data(), 4*2*4);

    }

private:
    vec2_t ps, fs;
    origin_t orig;
    mode_t m;
    float r;
};

}