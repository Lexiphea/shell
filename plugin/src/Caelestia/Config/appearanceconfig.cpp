#include "appearanceconfig.hpp"

#include "tokens.hpp"

namespace caelestia::shell::config {

namespace {

// Helper: funnel every token change and the local scale into a single valuesChanged signal
template <typename Source, typename Target> void connectTokenSignals(Source* source, Target* target) {
    QObject::connect(source, &Source::optionChanged, target, &Target::valuesChanged);
    QObject::connect(target, &Target::scaleChanged, target, &Target::valuesChanged);
}

} // namespace

#define SCALED_TOKEN(Class, name)                                                                                      \
    int Class::name() const {                                                                                          \
        return m_tokens ? static_cast<int>(m_tokens->name() * m_scale) : 0;                                            \
    }

// AppearanceRounding

void AppearanceRounding::bindTokens(RoundingTokens* tokens) {
    m_tokens = tokens;
    connectTokenSignals(tokens, this);
}

SCALED_TOKEN(AppearanceRounding, extraSmall)
SCALED_TOKEN(AppearanceRounding, small)
SCALED_TOKEN(AppearanceRounding, medium)
SCALED_TOKEN(AppearanceRounding, large)
SCALED_TOKEN(AppearanceRounding, largeIncreased)
SCALED_TOKEN(AppearanceRounding, extraLarge)
SCALED_TOKEN(AppearanceRounding, extraLargeIncreased)
SCALED_TOKEN(AppearanceRounding, extraExtraLarge)

int AppearanceRounding::full() const {
    return m_tokens ? m_tokens->full() : 0;
}

// AppearanceSpacing

void AppearanceSpacing::bindTokens(SpacingTokens* tokens) {
    m_tokens = tokens;
    connectTokenSignals(tokens, this);
}

SCALED_TOKEN(AppearanceSpacing, extraSmall)
SCALED_TOKEN(AppearanceSpacing, small)
SCALED_TOKEN(AppearanceSpacing, medium)
SCALED_TOKEN(AppearanceSpacing, large)
SCALED_TOKEN(AppearanceSpacing, largeIncreased)
SCALED_TOKEN(AppearanceSpacing, extraLarge)
SCALED_TOKEN(AppearanceSpacing, extraLargeIncreased)
SCALED_TOKEN(AppearanceSpacing, extraExtraLarge)

// AppearancePadding

void AppearancePadding::bindTokens(PaddingTokens* tokens) {
    m_tokens = tokens;
    connectTokenSignals(tokens, this);
}

SCALED_TOKEN(AppearancePadding, extraSmall)
SCALED_TOKEN(AppearancePadding, small)
SCALED_TOKEN(AppearancePadding, medium)
SCALED_TOKEN(AppearancePadding, large)
SCALED_TOKEN(AppearancePadding, largeIncreased)
SCALED_TOKEN(AppearancePadding, extraLarge)
SCALED_TOKEN(AppearancePadding, extraLargeIncreased)
SCALED_TOKEN(AppearancePadding, extraExtraLarge)

// AnimDurations

void AnimDurations::bindTokens(AnimDurationTokens* tokens) {
    m_tokens = tokens;
    connectTokenSignals(tokens, this);
}

SCALED_TOKEN(AnimDurations, small)
SCALED_TOKEN(AnimDurations, normal)
SCALED_TOKEN(AnimDurations, large)
SCALED_TOKEN(AnimDurations, extraLarge)
SCALED_TOKEN(AnimDurations, expressiveFastSpatial)
SCALED_TOKEN(AnimDurations, expressiveDefaultSpatial)
SCALED_TOKEN(AnimDurations, expressiveSlowSpatial)
SCALED_TOKEN(AnimDurations, expressiveFastEffects)
SCALED_TOKEN(AnimDurations, expressiveDefaultEffects)
SCALED_TOKEN(AnimDurations, expressiveSlowEffects)

#undef SCALED_TOKEN

} // namespace caelestia::shell::config
