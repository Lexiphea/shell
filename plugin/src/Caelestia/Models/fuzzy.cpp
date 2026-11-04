#include "fuzzy.hpp"

#include <fuzzy/fuzzy-searchable.hpp>

#include <algorithm>

namespace caelestia::shell::models {

Fuzzy::Fuzzy(QObject* parent)
    : QObject(parent) {}

QObjectList Fuzzy::search(const QObjectList& items, const QString& query, const QString& key) {
    const fuzzy::Query fuzzyQuery(query.toStdString());
    if (fuzzyQuery.empty())
        return items;

    struct Match {
        QObject* item;
        int score;
    };

    const auto keyName = key.toLatin1();
    QList<Match> matches;
    for (auto* item : items) {
        const auto text = item->property(keyName.constData()).toString().toStdString();
        if (const auto match = fuzzy::scoreWeighted({ { text, 1.0 } }, fuzzyQuery); match.accepted())
            matches.append({ item, match.score });
    }

    // Stable so ties keep the original order
    std::ranges::stable_sort(matches, [](const Match& a, const Match& b) {
        return a.score > b.score;
    });

    QObjectList results;
    results.reserve(matches.size());
    for (const auto& match : std::as_const(matches))
        results << match.item;
    return results;
}

} // namespace caelestia::shell::models
