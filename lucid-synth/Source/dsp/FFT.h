// Minimal radix-2 complex FFT used for wavetable generation (offline, not realtime critical)
#pragma once
#include <complex>
#include <vector>
#include <cmath>

namespace lucid {

class FFT
{
public:
    using cpx = std::complex<float>;

    explicit FFT (int size) : n (size)
    {
        // size must be power of two
        int l = 0; while ((1 << l) < n) ++l;
        logN = l;
        twiddles.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
        {
            const double a = -2.0 * 3.14159265358979323846 * (double) i / (double) n;
            twiddles[(size_t) i] = cpx ((float) std::cos (a), (float) std::sin (a));
        }
        bitrev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < logN; ++b) if (i & (1 << b)) r |= 1 << (logN - 1 - b);
            bitrev[(size_t) i] = r;
        }
    }

    // In-place. inverse=true computes the unscaled inverse transform.
    void perform (std::vector<cpx>& data, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = bitrev[(size_t) i];
            if (j > i) std::swap (data[(size_t) i], data[(size_t) j]);
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2;
            const int step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    cpx w = twiddles[(size_t) (k * step)];
                    if (inverse) w = std::conj (w);
                    const cpx u = data[(size_t) (i + k)];
                    const cpx v = data[(size_t) (i + k + half)] * w;
                    data[(size_t) (i + k)]        = u + v;
                    data[(size_t) (i + k + half)] = u - v;
                }
            }
        }
    }

    int size() const { return n; }

private:
    int n, logN = 0;
    std::vector<cpx> twiddles;
    std::vector<int> bitrev;
};

} // namespace lucid
