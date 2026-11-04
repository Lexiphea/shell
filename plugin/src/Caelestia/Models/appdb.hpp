#pragma once

#include <qhash.h>
#include <qobject.h>
#include <qqmlintegration.h>
#include <qqmllist.h>
#include <qregularexpression.h>
#include <qtimer.h>

#include <string>
#include <unordered_map>

namespace caelestia::shell::models {

class AppEntry : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AppEntry instances can only be retrieved from an AppDb")

    // The actual DesktopEntry, but we don't have access to the type so it's a QObject
    Q_PROPERTY(QObject* entry READ entry CONSTANT)

    Q_PROPERTY(quint32 frequency READ frequency NOTIFY frequencyChanged)
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString comment READ comment NOTIFY commentChanged)
    Q_PROPERTY(QString execString READ execString NOTIFY execStringChanged)
    Q_PROPERTY(QString startupClass READ startupClass NOTIFY startupClassChanged)
    Q_PROPERTY(QString genericName READ genericName NOTIFY genericNameChanged)
    Q_PROPERTY(QString categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(QString keywords READ keywords NOTIFY keywordsChanged)

public:
    explicit AppEntry(QObject* entry, quint32 frequency, qint64 lastUsed, QObject* parent = nullptr);

    [[nodiscard]] QObject* entry() const;

    [[nodiscard]] quint32 frequency() const;
    void setUsage(quint32 frequency, qint64 lastUsed);
    void registerLaunch(qint64 time);

    // How often and recently the app was launched, in [0, 1]
    [[nodiscard]] double frecency(qint64 now) const;

    [[nodiscard]] QString id() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QString comment() const;
    [[nodiscard]] QString execString() const;
    [[nodiscard]] QString startupClass() const;
    [[nodiscard]] QString genericName() const;
    [[nodiscard]] QString categories() const;
    [[nodiscard]] QString keywords() const;

    // UTF-8 text of a field, cached for matching
    [[nodiscard]] const std::string& searchText(const QString& field) const;

signals:
    void frequencyChanged();
    void nameChanged();
    void commentChanged();
    void execStringChanged();
    void startupClassChanged();
    void genericNameChanged();
    void categoriesChanged();
    void keywordsChanged();

private:
    QObject* m_entry;
    quint32 m_frequency;
    qint64 m_lastUsed; // Unix seconds, 0 if never launched
    // Node based so references from searchText stay valid when more fields are cached
    mutable std::unordered_map<QString, std::string> m_searchTexts;

    [[nodiscard]] QVariant entryProperty(const char* name) const;
};

class AppDb : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString uuid READ uuid CONSTANT)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged REQUIRED)
    Q_PROPERTY(QObjectList entries READ entries WRITE setEntries NOTIFY entriesChanged REQUIRED)
    Q_PROPERTY(QStringList favouriteApps READ favouriteApps WRITE setFavouriteApps NOTIFY favouriteAppsChanged REQUIRED)
    Q_PROPERTY(QQmlListProperty<caelestia::shell::models::AppEntry> apps READ apps NOTIFY appsChanged)

public:
    explicit AppDb(QObject* parent = nullptr);

    [[nodiscard]] QString uuid() const;

    [[nodiscard]] QString path() const;
    void setPath(const QString& path);

    [[nodiscard]] QObjectList entries() const;
    void setEntries(const QObjectList& entries);

    [[nodiscard]] QStringList favouriteApps() const;
    void setFavouriteApps(const QStringList& favApps);

    [[nodiscard]] QQmlListProperty<AppEntry> apps();

    // Records a launch of an app, or a use of a command by its usageId
    Q_INVOKABLE void registerLaunch(const QString& id);

    // Entries matching the query, best match and most used first. A field limits matching to that field.
    // Commands (objects with name, desc and usageId properties) are ranked in with the apps
    [[nodiscard]] Q_INVOKABLE QObjectList search(
        const QString& query, const QString& field = {}, const QObjectList& commands = {}) const;

signals:
    void pathChanged();
    void entriesChanged();
    void favouriteAppsChanged();
    void appsChanged();

private:
    QTimer* m_timer;

    const QString m_uuid;
    QString m_path;
    QObjectList m_entries;
    QStringList m_favouriteApps;                    // unedited string list from qml
    QList<QRegularExpression> m_favouriteAppsRegex; // pre-regexified m_favouriteApps list
    QHash<QString, AppEntry*> m_apps;
    QList<AppEntry*> m_sortedApps;
    mutable QHash<QString, std::pair<quint32, qint64>> m_commandUsage; // usageId -> frequency, last used

    static QString regexifyString(const QString& original);
    bool sortApps();
    bool isFavourite(const AppEntry* app) const;
    std::pair<quint32, qint64> getUsage(const QString& id) const;
    void updateAppUsage();
    void updateApps();
};

} // namespace caelestia::shell::models
