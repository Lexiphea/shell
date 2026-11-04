#include "appdb.hpp"

#include <qdatetime.h>
#include <qloggingcategory.h>
#include <qqmlengine.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qsqlrecord.h>
#include <quuid.h>

#include <fuzzy/fuzzy-searchable.hpp>

#include <algorithm>

using Qt::StringLiterals::operator""_s;

namespace {

Q_LOGGING_CATEGORY(lcAppDb, "caelestia.appdb", QtInfoMsg)

void initDb(const QSqlDatabase& db) {
    QSqlQuery query(db);
    query.exec(u"CREATE TABLE IF NOT EXISTS frequencies (id TEXT PRIMARY KEY, frequency INTEGER, last_used INTEGER)"_s);

    // Databases from before launch times were tracked
    if (!db.record(u"frequencies"_s).contains(u"last_used"_s))
        query.exec(u"ALTER TABLE frequencies ADD COLUMN last_used INTEGER"_s);
}

double frecency(quint32 frequency, qint64 lastUsed, qint64 now) {
    return fuzzy::frecency(
        frequency, lastUsed > 0 ? std::optional(static_cast<std::uint64_t>(lastUsed)) : std::nullopt, now);
}

} // namespace

namespace caelestia::shell::models {

AppEntry::AppEntry(QObject* entry, quint32 frequency, qint64 lastUsed, QObject* parent)
    : QObject(parent)
    , m_entry(entry)
    , m_frequency(frequency)
    , m_lastUsed(lastUsed) {
    const auto* mo = m_entry->metaObject();
    const auto* tmo = &AppEntry::staticMetaObject;

    for (const auto& prop :
        { "name", "comment", "execString", "startupClass", "genericName", "categories", "keywords" }) {
        const auto metaProp = mo->property(mo->indexOfProperty(prop));
        const auto thisMetaProp = tmo->property(tmo->indexOfProperty(prop));
        QObject::connect(m_entry, metaProp.notifySignal(), this, thisMetaProp.notifySignal());
    }

    const auto clearSearchTexts = [this] {
        m_searchTexts.clear();
    };
    QObject::connect(this, &AppEntry::nameChanged, this, clearSearchTexts);
    QObject::connect(this, &AppEntry::commentChanged, this, clearSearchTexts);
    QObject::connect(this, &AppEntry::execStringChanged, this, clearSearchTexts);
    QObject::connect(this, &AppEntry::startupClassChanged, this, clearSearchTexts);
    QObject::connect(this, &AppEntry::genericNameChanged, this, clearSearchTexts);
    QObject::connect(this, &AppEntry::categoriesChanged, this, clearSearchTexts);
    QObject::connect(this, &AppEntry::keywordsChanged, this, clearSearchTexts);

    // Entries are handed straight to QML by AppDb::search, so stop the JS GC claiming them
    QQmlEngine::setObjectOwnership(m_entry, QQmlEngine::CppOwnership);

    QObject::connect(m_entry, &QObject::destroyed, this, [this] {
        m_entry = nullptr;
        deleteLater();
    });
}

QObject* AppEntry::entry() const {
    return m_entry;
}

quint32 AppEntry::frequency() const {
    return m_frequency;
}

void AppEntry::setUsage(quint32 frequency, qint64 lastUsed) {
    m_lastUsed = lastUsed;
    if (m_frequency != frequency) {
        m_frequency = frequency;
        emit frequencyChanged();
    }
}

void AppEntry::registerLaunch(qint64 time) {
    m_lastUsed = time;
    m_frequency++;
    emit frequencyChanged();
}

double AppEntry::frecency(qint64 now) const {
    return ::frecency(m_frequency, m_lastUsed, now);
}

const std::string& AppEntry::searchText(const QString& field) const {
    auto [it, inserted] = m_searchTexts.try_emplace(field);
    if (inserted)
        it->second = property(field.toLatin1().constData()).toString().toStdString();
    return it->second;
}

QVariant AppEntry::entryProperty(const char* name) const {
    return m_entry ? m_entry->property(name) : QVariant();
}

QString AppEntry::id() const {
    return entryProperty("id").toString();
}

QString AppEntry::name() const {
    return entryProperty("name").toString();
}

QString AppEntry::comment() const {
    return entryProperty("comment").toString();
}

QString AppEntry::execString() const {
    return entryProperty("execString").toString();
}

QString AppEntry::startupClass() const {
    return entryProperty("startupClass").toString();
}

QString AppEntry::genericName() const {
    return entryProperty("genericName").toString();
}

QString AppEntry::categories() const {
    return entryProperty("categories").toStringList().join(u" "_s);
}

QString AppEntry::keywords() const {
    return entryProperty("keywords").toStringList().join(u" "_s);
}

AppDb::AppDb(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_uuid(QUuid::createUuid().toString()) {
    m_timer->setSingleShot(true);
    m_timer->setInterval(300);
    QObject::connect(m_timer, &QTimer::timeout, this, &AppDb::updateApps);

    auto db = QSqlDatabase::addDatabase(u"QSQLITE"_s, m_uuid);
    db.setDatabaseName(u":memory:"_s);
    db.open();
    initDb(db);
}

QString AppDb::uuid() const {
    return m_uuid;
}

QString AppDb::path() const {
    return m_path;
}

void AppDb::setPath(const QString& path) {
    const auto newPath = path.isEmpty() ? u":memory:"_s : path;

    if (m_path == newPath) {
        return;
    }

    m_path = newPath;
    emit pathChanged();

    auto db = QSqlDatabase::database(m_uuid, false);
    db.close();
    db.setDatabaseName(newPath);
    db.open();
    initDb(db);

    m_commandUsage.clear();
    updateAppUsage();
}

QObjectList AppDb::entries() const {
    return m_entries;
}

void AppDb::setEntries(const QObjectList& entries) {
    if (m_entries == entries) {
        return;
    }

    m_entries = entries;
    emit entriesChanged();

    m_timer->start();
}

QStringList AppDb::favouriteApps() const {
    return m_favouriteApps;
}

void AppDb::setFavouriteApps(const QStringList& favApps) {
    if (m_favouriteApps == favApps) {
        return;
    }

    m_favouriteApps = favApps;
    emit favouriteAppsChanged();
    m_favouriteAppsRegex.clear();
    m_favouriteAppsRegex.reserve(m_favouriteApps.size());
    for (const QString& item : std::as_const(m_favouriteApps)) {
        const QRegularExpression re(regexifyString(item));
        if (re.isValid()) {
            m_favouriteAppsRegex << re;
        } else {
            qCWarning(lcAppDb) << "setFavouriteApps: regular expression is not valid:" << re.pattern();
        }
    }

    if (sortApps()) {
        emit appsChanged();
    }
}

QString AppDb::regexifyString(const QString& original) {
    if (original.startsWith(u'^') && original.endsWith(u'$'))
        return original;

    const QString escaped = QRegularExpression::escape(original);
    return u"^%1$"_s.arg(escaped);
}

QQmlListProperty<AppEntry> AppDb::apps() {
    return { this, &m_sortedApps };
}

void AppDb::registerLaunch(const QString& id) {
    const auto now = QDateTime::currentSecsSinceEpoch();
    const auto db = QSqlDatabase::database(m_uuid);
    QSqlQuery query(db);

    query.prepare(u"INSERT INTO frequencies (id, frequency, last_used) "
                  "VALUES (:id, 1, :now) "
                  "ON CONFLICT (id) DO UPDATE SET frequency = frequency + 1, last_used = :now"_s);
    query.bindValue(u":id"_s, id);
    query.bindValue(u":now"_s, now);
    query.exec();

    auto* app = m_apps.value(id);
    if (app) {
        app->registerLaunch(now);
        if (sortApps()) {
            emit appsChanged();
        }
    } else {
        // Not an app, so a command. Reloaded from the database on its next search
        m_commandUsage.remove(id);
    }
}

QObjectList AppDb::search(const QString& query, const QString& field, const QObjectList& commands) const {
    QObjectList results;
    const fuzzy::Query fuzzyQuery(query.toStdString());

    if (fuzzyQuery.empty()) {
        results.reserve(m_sortedApps.size() + commands.size());
        for (const auto* app : m_sortedApps) {
            if (app->entry())
                results << app->entry();
        }
        return results << commands;
    }

    struct Match {
        QObject* item;
        double score;
    };

    const auto now = QDateTime::currentSecsSinceEpoch();
    QList<Match> matches;
    for (auto* app : m_sortedApps) {
        if (!app->entry())
            continue;

        // Same fields and weights as vicinae, plus the generic name so "browser" finds browsers
        const auto match =
            field.isEmpty()
                ? fuzzy::scoreWeighted(
                      { { app->searchText(u"name"_s), 1.0 }, { app->searchText(u"genericName"_s), 0.6 },
                          { app->searchText(u"keywords"_s), 0.6 }, { app->searchText(u"categories"_s), 0.6 } },
                      fuzzyQuery)
                : fuzzy::scoreWeighted({ { app->searchText(field), 1.0 } }, fuzzyQuery);
        if (match.accepted())
            matches.append({ app->entry(), match.score + fuzzy::FRECENCY_WEIGHT * app->frecency(now) });
    }

    // Scored on the same scale, so "lock" ranks the Lock command over Deadlock
    for (auto* command : commands) {
        const auto match = fuzzy::scoreWeighted({ { command->property("name").toString().toStdString(), 1.0 },
                                                    { command->property("desc").toString().toStdString(), 0.5 } },
            fuzzyQuery);
        if (!match.accepted())
            continue;

        const auto id = command->property("usageId").toString();
        auto it = m_commandUsage.find(id);
        if (it == m_commandUsage.end())
            it = m_commandUsage.insert(id, getUsage(id));
        matches.append({ command, match.score + fuzzy::FRECENCY_WEIGHT * frecency(it->first, it->second, now) });
    }

    // Stable so ties keep the favourite/frecency order, with apps before commands
    std::ranges::stable_sort(matches, [](const Match& a, const Match& b) {
        return a.score > b.score;
    });

    results.reserve(matches.size());
    for (const auto& match : std::as_const(matches))
        results << match.item;
    return results;
}

bool AppDb::sortApps() {
    auto sorted = m_apps.values();
    const auto now = QDateTime::currentSecsSinceEpoch();

    // Pre-compute favourite status and frecency to avoid recomputing them during sort
    QSet<const AppEntry*> favSet;
    QHash<const AppEntry*, double> frecencies;
    favSet.reserve(sorted.size());
    frecencies.reserve(sorted.size());
    for (const auto* app : std::as_const(sorted)) {
        if (isFavourite(app))
            favSet.insert(app);
        frecencies.insert(app, app->frecency(now));
    }

    std::ranges::sort(sorted, [&favSet, &frecencies](AppEntry* a, AppEntry* b) {
        const bool aIsFav = favSet.contains(a);
        const bool bIsFav = favSet.contains(b);
        if (aIsFav != bIsFav)
            return aIsFav;
        if (const auto order = frecencies.value(a) <=> frecencies.value(b); order != 0)
            return order > 0;
        return a->name().localeAwareCompare(b->name()) < 0;
    });

    if (sorted == m_sortedApps)
        return false;
    m_sortedApps = sorted;
    return true;
}

bool AppDb::isFavourite(const AppEntry* app) const {
    const QString id = app->id();
    return std::ranges::any_of(m_favouriteAppsRegex, [&id](const QRegularExpression& re) {
        return re.match(id).hasMatch();
    });
}

std::pair<quint32, qint64> AppDb::getUsage(const QString& id) const {
    const auto db = QSqlDatabase::database(m_uuid);
    QSqlQuery query(db);

    query.prepare(u"SELECT frequency, last_used FROM frequencies WHERE id = :id"_s);
    query.bindValue(u":id"_s, id);

    if (query.exec() && query.next()) {
        return { query.value(0).toUInt(), query.value(1).toLongLong() };
    }

    return {};
}

void AppDb::updateAppUsage() {
    for (auto* app : std::as_const(m_apps)) {
        const auto [frequency, lastUsed] = getUsage(app->id());
        app->setUsage(frequency, lastUsed);
    }

    if (sortApps()) {
        emit appsChanged();
    }
}

void AppDb::updateApps() {
    bool dirty = false;
    QSet<QString> newIds;

    for (const auto& entry : std::as_const(m_entries)) {
        const auto id = entry->property("id").toString();
        newIds.insert(id);
        if (!m_apps.contains(id)) {
            dirty = true;
            const auto [frequency, lastUsed] = getUsage(id);
            auto* const newEntry = new AppEntry(entry, frequency, lastUsed, this);
            QObject::connect(newEntry, &QObject::destroyed, this, [id, this] {
                if (m_apps.remove(id)) {
                    sortApps();
                    emit appsChanged();
                }
            });
            m_apps.insert(id, newEntry);
        }
    }

    for (auto it = m_apps.begin(); it != m_apps.end();) {
        if (!newIds.contains(it.key())) {
            dirty = true;
            it.value()->deleteLater();
            it = m_apps.erase(it);
        } else {
            ++it;
        }
    }

    if (dirty) {
        sortApps();
        emit appsChanged();
    }
}

} // namespace caelestia::shell::models
