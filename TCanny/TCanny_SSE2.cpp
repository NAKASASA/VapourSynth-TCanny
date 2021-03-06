#ifdef VS_TARGET_CPU_X86
#include "TCanny.hpp"

template<typename T>
static void copyPlane(const T * srcp, float * dstp, const int width, const int height, const int srcStride, const int dstStride, const float offset) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 4) {
            if (std::is_same<T, uint8_t>::value)
                to_float(Vec4i().load_4uc(srcp + x)).stream(dstp + x);
            else if (std::is_same<T, uint16_t>::value)
                to_float(Vec4i().load_4us(srcp + x)).stream(dstp + x);
            else
                (Vec4f().load_a(srcp + x) + offset).stream(dstp + x);
        }

        srcp += srcStride;
        dstp += dstStride;
    }
}

template<typename T>
static void gaussianBlur(const T * __srcp, float * temp, float * dstp, const float * weightsH, const float * weightsV, const int width, const int height,
                         const int srcStride, const int dstStride, const int radiusH, const int radiusV, const float offset) noexcept {
    const int diameter = radiusV * 2 + 1;
    const T ** _srcp = new const T *[diameter];

    _srcp[radiusV] = __srcp;
    for (int i = 1; i <= radiusV; i++) {
        _srcp[radiusV - i] = _srcp[radiusV - 1 + i];
        _srcp[radiusV + i] = _srcp[radiusV] + srcStride * i;
    }

    weightsH += radiusH;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 4) {
            Vec4f sum = zero_4f();

            for (int i = 0; i < diameter; i++) {
                if (std::is_same<T, uint8_t>::value) {
                    const Vec4f srcp = to_float(Vec4i().load_4uc(_srcp[i] + x));
                    sum = mul_add(srcp, weightsV[i], sum);
                } else if (std::is_same<T, uint16_t>::value) {
                    const Vec4f srcp = to_float(Vec4i().load_4us(_srcp[i] + x));
                    sum = mul_add(srcp, weightsV[i], sum);
                } else {
                    const Vec4f srcp = Vec4f().load_a(_srcp[i] + x);
                    sum = mul_add(srcp + offset, weightsV[i], sum);
                }
            }

            sum.store_a(temp + x);
        }

        for (int i = 1; i <= radiusH; i++) {
            temp[-i] = temp[-1 + i];
            temp[width - 1 + i] = temp[width - i];
        }

        for (int x = 0; x < width; x += 4) {
            Vec4f sum = zero_4f();

            for (int i = -radiusH; i <= radiusH; i++) {
                const Vec4f srcp = Vec4f().load(temp + x + i);
                sum = mul_add(srcp, weightsH[i], sum);
            }

            sum.stream(dstp + x);
        }

        for (int i = 0; i < diameter - 1; i++)
            _srcp[i] = _srcp[i + 1];
        if (y < height - 1 - radiusV)
            _srcp[diameter - 1] += srcStride;
        else if (y > height - 1 - radiusV)
            _srcp[diameter - 1] -= srcStride;
        dstp += dstStride;
    }

    delete[] _srcp;
}

template<typename T>
static void gaussianBlurV(const T * __srcp, float * dstp, const float * weights, const int width, const int height, const int srcStride, const int dstStride,
                          const int radius, const float offset) noexcept {
    const int diameter = radius * 2 + 1;
    const T ** _srcp = new const T *[diameter];

    _srcp[radius] = __srcp;
    for (int i = 1; i <= radius; i++) {
        _srcp[radius - i] = _srcp[radius - 1 + i];
        _srcp[radius + i] = _srcp[radius] + srcStride * i;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 4) {
            Vec4f sum = zero_4f();

            for (int i = 0; i < diameter; i++) {
                if (std::is_same<T, uint8_t>::value) {
                    const Vec4f srcp = to_float(Vec4i().load_4uc(_srcp[i] + x));
                    sum = mul_add(srcp, weights[i], sum);
                } else if (std::is_same<T, uint16_t>::value) {
                    const Vec4f srcp = to_float(Vec4i().load_4us(_srcp[i] + x));
                    sum = mul_add(srcp, weights[i], sum);
                } else {
                    const Vec4f srcp = Vec4f().load_a(_srcp[i] + x);
                    sum = mul_add(srcp + offset, weights[i], sum);
                }
            }

            sum.stream(dstp + x);
        }

        for (int i = 0; i < diameter - 1; i++)
            _srcp[i] = _srcp[i + 1];
        if (y < height - 1 - radius)
            _srcp[diameter - 1] += srcStride;
        else if (y > height - 1 - radius)
            _srcp[diameter - 1] -= srcStride;
        dstp += dstStride;
    }

    delete[] _srcp;
}

template<typename T>
static void gaussianBlurH(const T * _srcp, float * temp, float * dstp, const float * weights, const int width, const int height,
                          const int srcStride, const int dstStride, const int radius, const float offset) noexcept {
    weights += radius;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 4) {
            if (std::is_same<T, uint8_t>::value)
                to_float(Vec4i().load_4uc(_srcp + x)).store_a(temp + x);
            else if (std::is_same<T, uint16_t>::value)
                to_float(Vec4i().load_4us(_srcp + x)).store_a(temp + x);
            else
                (Vec4f().load_a(_srcp + x) + offset).store_a(temp + x);
        }

        for (int i = 1; i <= radius; i++) {
            temp[-i] = temp[-1 + i];
            temp[width - 1 + i] = temp[width - i];
        }

        for (int x = 0; x < width; x += 4) {
            Vec4f sum = zero_4f();

            for (int i = -radius; i <= radius; i++) {
                const Vec4f srcp = Vec4f().load(temp + x + i);
                sum = mul_add(srcp, weights[i], sum);
            }

            sum.stream(dstp + x);
        }

        _srcp += srcStride;
        dstp += dstStride;
    }
}

static void detectEdge(float * blur, float * gradient, unsigned * direction, const int width, const int height, const int stride, const int bgStride,
                       const int mode, const int op) noexcept {
    float * srcpp = blur;
    float * srcp = blur;
    float * srcpn = blur + bgStride;

    srcp[-1] = srcp[0];
    srcp[width] = srcp[width - 1];

    for (int y = 0; y < height; y++) {
        srcpn[-1] = srcpn[0];
        srcpn[width] = srcpn[width - 1];

        for (int x = 0; x < width; x += 4) {
            const Vec4f topLeft = Vec4f().load(srcpp + x - 1);
            const Vec4f top = Vec4f().load_a(srcpp + x);
            const Vec4f topRight = Vec4f().load(srcpp + x + 1);
            const Vec4f left = Vec4f().load(srcp + x - 1);
            const Vec4f right = Vec4f().load(srcp + x + 1);
            const Vec4f bottomLeft = Vec4f().load(srcpn + x - 1);
            const Vec4f bottom = Vec4f().load_a(srcpn + x);
            const Vec4f bottomRight = Vec4f().load(srcpn + x + 1);

            Vec4f gx, gy;

            if (op == 0) {
                gx = right - left;
                gy = top - bottom;
            } else if (op == 1) {
                gx = (topRight + right + bottomRight - topLeft - left - bottomLeft) * 0.5f;
                gy = (topLeft + top + topRight - bottomLeft - bottom - bottomRight) * 0.5f;
            } else if (op == 2) {
                gx = topRight + mul_add(2.f, right, bottomRight) - topLeft - mul_add(2.f, left, bottomLeft);
                gy = topLeft + mul_add(2.f, top, topRight) - bottomLeft - mul_add(2.f, bottom, bottomRight);
            } else {
                gx = mul_add(3.f, topRight, mul_add(10.f, right, 3.f * bottomRight)) - mul_add(3.f, topLeft, mul_add(10.f, left, 3.f * bottomLeft));
                gy = mul_add(3.f, topLeft, mul_add(10.f, top, 3.f * topRight)) - mul_add(3.f, bottomLeft, mul_add(10.f, bottom, 3.f * bottomRight));
            }

            sqrt(mul_add(gx, gx, gy * gy)).stream(gradient + x);

            if (mode == 0) {
                Vec4f dr = atan2(gy, gx);
                dr = if_add(dr < 0.f, dr, M_PIF);

                const Vec4ui bin = Vec4ui(truncate_to_int(mul_add(dr, 4.f * M_1_PIF, 0.5f)));
                select(bin >= 4, zero_128b(), bin).stream(direction + x);
            }
        }

        srcpp = srcp;
        srcp = srcpn;
        if (y < height - 2)
            srcpn += bgStride;
        gradient += bgStride;
        direction += stride;
    }
}

static void nonMaximumSuppression(const unsigned * _direction, float * _gradient, float * blur, const int width, const int height,
                                  const int stride, const int bgStride, const int radiusAlign) noexcept {
    _gradient[-1] = _gradient[0];
    _gradient[-1 + bgStride * (height - 1)] = _gradient[bgStride * (height - 1)];
    _gradient[width] = _gradient[width - 1];
    _gradient[width + bgStride * (height - 1)] = _gradient[width - 1 + bgStride * (height - 1)];
    std::copy_n(_gradient - radiusAlign, width + radiusAlign * 2, _gradient - radiusAlign - bgStride);
    std::copy_n(_gradient - radiusAlign + bgStride * (height - 1), width + radiusAlign * 2, _gradient - radiusAlign + bgStride * height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 4) {
            const Vec4ui direction = Vec4ui().load_a(_direction + x);

            Vec4fb mask = Vec4fb(direction == 0);
            Vec4f gradient = max(Vec4f().load(_gradient + x + 1), Vec4f().load(_gradient + x - 1));
            Vec4f result = gradient & mask;

            mask = Vec4fb(direction == 1);
            gradient = max(Vec4f().load(_gradient + x - bgStride + 1), Vec4f().load(_gradient + x + bgStride - 1));
            result |= gradient & mask;

            mask = Vec4fb(direction == 2);
            gradient = max(Vec4f().load_a(_gradient + x - bgStride), Vec4f().load_a(_gradient + x + bgStride));
            result |= gradient & mask;

            mask = Vec4fb(direction == 3);
            gradient = max(Vec4f().load(_gradient + x - bgStride - 1), Vec4f().load(_gradient + x + bgStride + 1));
            result |= gradient & mask;

            gradient = Vec4f().load_a(_gradient + x);
            select(gradient >= result, gradient, fltLowest).stream(blur + x);
        }

        _direction += stride;
        _gradient += bgStride;
        blur += bgStride;
    }
}

template<typename T> static void outputGB(const float *, T *, const int, const int, const int, const int, const uint16_t, const float) noexcept;

template<>
void outputGB(const float * _srcp, uint8_t * dstp, const int width, const int height, const int srcStride, const int dstStride, const uint16_t peak, const float offset) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            const Vec4i srcp_4i_0 = truncate_to_int(Vec4f().load_a(_srcp + x) + 0.5f);
            const Vec4i srcp_4i_1 = truncate_to_int(Vec4f().load_a(_srcp + x + 4) + 0.5f);
            const Vec4i srcp_4i_2 = truncate_to_int(Vec4f().load_a(_srcp + x + 8) + 0.5f);
            const Vec4i srcp_4i_3 = truncate_to_int(Vec4f().load_a(_srcp + x + 12) + 0.5f);
            const Vec8s srcp_8s_0 = compress_saturated(srcp_4i_0, srcp_4i_1);
            const Vec8s srcp_8s_1 = compress_saturated(srcp_4i_2, srcp_4i_3);
            const Vec16uc srcp = compress_saturated_s2u(srcp_8s_0, srcp_8s_1);
            srcp.stream(dstp + x);
        }

        _srcp += srcStride;
        dstp += dstStride;
    }
}

template<>
void outputGB(const float * _srcp, uint16_t * dstp, const int width, const int height, const int srcStride, const int dstStride, const uint16_t peak, const float offset) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            const Vec4i srcp_4i_0 = truncate_to_int(Vec4f().load_a(_srcp + x) + 0.5f);
            const Vec4i srcp_4i_1 = truncate_to_int(Vec4f().load_a(_srcp + x + 4) + 0.5f);
            const Vec8us srcp = compress_saturated_s2u(srcp_4i_0, srcp_4i_1);
            min(srcp, peak).stream(dstp + x);
        }

        _srcp += srcStride;
        dstp += dstStride;
    }
}

template<>
void outputGB(const float * _srcp, float * dstp, const int width, const int height, const int srcStride, const int dstStride, const uint16_t peak, const float offset) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 4) {
            const Vec4f srcp = Vec4f().load_a(_srcp + x);
            (srcp - offset).stream(dstp + x);
        }

        _srcp += srcStride;
        dstp += dstStride;
    }
}

template<typename T> static void binarizeCE(const float *, T *, const int, const int, const int, const int, const uint16_t, const float, const float) noexcept;

template<>
void binarizeCE(const float * srcp, uint8_t * dstp, const int width, const int height, const int srcStride, const int dstStride,
                const uint16_t peak, const float lower, const float upper) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            const Vec4ib mask_4ib_0 = Vec4ib(Vec4f().load_a(srcp + x) == fltMax);
            const Vec4ib mask_4ib_1 = Vec4ib(Vec4f().load_a(srcp + x + 4) == fltMax);
            const Vec4ib mask_4ib_2 = Vec4ib(Vec4f().load_a(srcp + x + 8) == fltMax);
            const Vec4ib mask_4ib_3 = Vec4ib(Vec4f().load_a(srcp + x + 12) == fltMax);
            const Vec8sb mask_8sb_0 = Vec8sb(compress_saturated(mask_4ib_0, mask_4ib_1));
            const Vec8sb mask_8sb_1 = Vec8sb(compress_saturated(mask_4ib_2, mask_4ib_3));
            const Vec16cb mask = Vec16cb(compress_saturated(mask_8sb_0, mask_8sb_1));
            select(mask, Vec16uc(255), zero_128b()).stream(dstp + x);
        }

        srcp += srcStride;
        dstp += dstStride;
    }
}

template<>
void binarizeCE(const float * srcp, uint16_t * dstp, const int width, const int height, const int srcStride, const int dstStride,
                const uint16_t peak, const float lower, const float upper) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            const Vec4ib mask_4ib_0 = Vec4ib(Vec4f().load_a(srcp + x) == fltMax);
            const Vec4ib mask_4ib_1 = Vec4ib(Vec4f().load_a(srcp + x + 4) == fltMax);
            const Vec8sb mask = Vec8sb(compress_saturated(mask_4ib_0, mask_4ib_1));
            select(mask, Vec8us(peak), zero_128b()).stream(dstp + x);
        }

        srcp += srcStride;
        dstp += dstStride;
    }
}

template<>
void binarizeCE(const float * srcp, float * dstp, const int width, const int height, const int srcStride, const int dstStride,
                const uint16_t peak, const float lower, const float upper) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 4) {
            const Vec4fb mask = (Vec4f().load_a(srcp + x) == fltMax);
            select(mask, Vec4f(upper), Vec4f(lower)).stream(dstp + x);
        }

        srcp += srcStride;
        dstp += dstStride;
    }
}

template<typename T> static void discretizeGM(const float *, T *, const int, const int, const int, const int, const float, const uint16_t, const float) noexcept;

template<>
void discretizeGM(const float * _srcp, uint8_t * dstp, const int width, const int height, const int srcStride, const int dstStride,
                  const float magnitude, const uint16_t peak, const float offset) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 16) {
            const Vec4f srcp_4f_0 = Vec4f().load_a(_srcp + x);
            const Vec4f srcp_4f_1 = Vec4f().load_a(_srcp + x + 4);
            const Vec4f srcp_4f_2 = Vec4f().load_a(_srcp + x + 8);
            const Vec4f srcp_4f_3 = Vec4f().load_a(_srcp + x + 12);
            const Vec4i srcp_4i_0 = truncate_to_int(mul_add(srcp_4f_0, magnitude, 0.5f));
            const Vec4i srcp_4i_1 = truncate_to_int(mul_add(srcp_4f_1, magnitude, 0.5f));
            const Vec4i srcp_4i_2 = truncate_to_int(mul_add(srcp_4f_2, magnitude, 0.5f));
            const Vec4i srcp_4i_3 = truncate_to_int(mul_add(srcp_4f_3, magnitude, 0.5f));
            const Vec8s srcp_8s_0 = compress_saturated(srcp_4i_0, srcp_4i_1);
            const Vec8s srcp_8s_1 = compress_saturated(srcp_4i_2, srcp_4i_3);
            const Vec16uc srcp = compress_saturated_s2u(srcp_8s_0, srcp_8s_1);
            srcp.stream(dstp + x);
        }

        _srcp += srcStride;
        dstp += dstStride;
    }
}

template<>
void discretizeGM(const float * _srcp, uint16_t * dstp, const int width, const int height, const int srcStride, const int dstStride,
                  const float magnitude, const uint16_t peak, const float offset) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            const Vec4f srcp_4f_0 = Vec4f().load_a(_srcp + x);
            const Vec4f srcp_4f_1 = Vec4f().load_a(_srcp + x + 4);
            const Vec4i srcp_4i_0 = truncate_to_int(mul_add(srcp_4f_0, magnitude, 0.5f));
            const Vec4i srcp_4i_1 = truncate_to_int(mul_add(srcp_4f_1, magnitude, 0.5f));
            const Vec8us srcp = compress_saturated_s2u(srcp_4i_0, srcp_4i_1);
            min(srcp, peak).stream(dstp + x);
        }

        _srcp += srcStride;
        dstp += dstStride;
    }
}

template<>
void discretizeGM(const float * _srcp, float * dstp, const int width, const int height, const int srcStride, const int dstStride,
                  const float magnitude, const uint16_t peak, const float offset) noexcept {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 4) {
            const Vec4f srcp = Vec4f().load_a(_srcp + x);
            mul_sub(srcp, magnitude, offset).stream(dstp + x);
        }

        _srcp += srcStride;
        dstp += dstStride;
    }
}

template<typename T>
void filter_sse2(const VSFrameRef * src, VSFrameRef * dst, const TCannyData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept {
    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int stride = vsapi->getStride(src, plane) / sizeof(T);
            const int bgStride = stride + d->radiusAlign * 2;
            const T * srcp = reinterpret_cast<const T *>(vsapi->getReadPtr(src, plane));
            T * dstp = reinterpret_cast<T *>(vsapi->getWritePtr(dst, plane));

            const auto threadId = std::this_thread::get_id();
            float * blur = d->blur.at(threadId) + d->radiusAlign;
            float * gradient = d->gradient.at(threadId) + bgStride + d->radiusAlign;
            unsigned * direction = d->direction.at(threadId);
            bool * found = d->found.at(threadId);

            if (d->radiusV[plane] && d->radiusH[plane])
                gaussianBlur(srcp, gradient, blur, d->weightsH[plane], d->weightsV[plane], width, height, stride, bgStride, d->radiusH[plane], d->radiusV[plane], d->offset[plane]);
            else if (d->radiusV[plane])
                gaussianBlurV(srcp, blur, d->weightsV[plane], width, height, stride, bgStride, d->radiusV[plane], d->offset[plane]);
            else if (d->radiusH[plane])
                gaussianBlurH(srcp, gradient, blur, d->weightsH[plane], width, height, stride, bgStride, d->radiusH[plane], d->offset[plane]);
            else
                copyPlane(srcp, blur, width, height, stride, bgStride, d->offset[plane]);

            if (d->mode != -1) {
                detectEdge(blur, gradient, direction, width, height, stride, bgStride, d->mode, d->op);

                if (d->mode == 0) {
                    nonMaximumSuppression(direction, gradient, blur, width, height, stride, bgStride, d->radiusAlign);
                    hysteresis(blur, found, width, height, bgStride, d->t_h, d->t_l);
                }
            }

            if (d->mode == -1)
                outputGB(blur, dstp, width, height, bgStride, stride, d->peak, d->offset[plane]);
            else if (d->mode == 0)
                binarizeCE(blur, dstp, width, height, bgStride, stride, d->peak, d->lower[plane], d->upper[plane]);
            else
                discretizeGM(gradient, dstp, width, height, bgStride, stride, d->magnitude, d->peak, d->offset[plane]);
        }
    }
}

template void filter_sse2<uint8_t>(const VSFrameRef *, VSFrameRef *, const TCannyData * const VS_RESTRICT, const VSAPI *) noexcept;
template void filter_sse2<uint16_t>(const VSFrameRef *, VSFrameRef *, const TCannyData * const VS_RESTRICT, const VSAPI *) noexcept;
template void filter_sse2<float>(const VSFrameRef *, VSFrameRef *, const TCannyData * const VS_RESTRICT, const VSAPI *) noexcept;
#endif
