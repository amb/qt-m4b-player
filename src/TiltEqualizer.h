#pragma once

#include <QString>
#include <cmath>
#include <algorithm>

// Same first-order tilt response as the reference player. The libmpv graph
// explicitly negotiates 48 kHz before applying these digital coefficients.
namespace TiltEqualizer {
constexpr double sampleRate = 48000;
struct Coefficients {
    double b0 = 1, b1 = 0, a1 = 0;
    double decibels(double frequency) const
    {
        const double cosine = std::cos(2 * std::acos(-1.0) * frequency / sampleRate);
        return 10 * std::log10(std::max(1e-20,
            (b0*b0 + b1*b1 + 2*b0*b1*cosine) / (1 + a1*a1 + 2*a1*cosine)));
    }
};
inline Coefficients coefficients(double tone, double frequency, bool headroom = true)
{
    tone = std::isfinite(tone) ? std::clamp(tone, -12.0, 12.0) : 0;
    frequency = std::isfinite(frequency) ? std::clamp(frequency, 200.0, 4000.0) : 1000;
    if (tone == 0)
        return {};
    const double g = std::tan(std::acos(-1.0) * frequency / sampleRate);
    const double h = std::pow(10, tone / 20);
    const double denominator = 1 + g*h;
    const double trim = headroom ? std::pow(10, -std::abs(tone) / 20) : 1;
    return {trim*(h+g)/denominator, trim*(g-h)/denominator, (g*h-1)/denominator};
}
inline QString filter(double tone, double frequency, double boost = 0)
{
    const auto c = coefficients(tone, frequency);
    QString graph = QStringLiteral("@speech-tone:lavfi=[aresample=48000,biquad=b0=%1:b1=%2:b2=0:a0=1:a1=%3:a2=0:precision=f64]")
        .arg(c.b0, 0, 'g', 17).arg(c.b1, 0, 'g', 17).arg(c.a1, 0, 'g', 17);
    boost = std::isfinite(boost) ? std::clamp(boost, 0.0, 24.0) : 0;
    if (boost > 0) {
        // Place pitch-preserving speed processing before the limiter, so speed
        // changes cannot create unprotected peaks. mpv uses this explicit
        // scaletempo2 instance instead of adding its automatic speed filter.
        // Four-times oversampling catches peaks between 48 kHz samples; leave
        // 1 dB of output margin. Disable alimiter's default normalization, which
        // would otherwise undo that margin. Latency compensation preserves PTS
        // and drains the lookahead at EOF.
        graph += QStringLiteral(
            ",@speech-speed:scaletempo2,@speech-boost:lavfi=[aresample=192000,"
            "alimiter=level_in=%1:limit=0.8912509381337456:attack=10:release=100:asc=1:level=0:latency=1,"
            "aresample=48000]").arg(std::pow(10, boost / 20), 0, 'g', 17);
    }
    return graph;
}
}
