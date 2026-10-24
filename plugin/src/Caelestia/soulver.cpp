#include "soulver.hpp"

#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qstandardpaths.h>

#include <cstdlib>
#include <dlfcn.h>

namespace caelestia {

using Qt::StringLiterals::operator""_s;

// SoulverCore must stay on the main thread: it asserts it is on the main dispatch queue (SIGILL otherwise), so
// loading, initialising or evaluating on a worker thread crashes the shell
Soulver::Soulver(QObject* parent)
    : QObject(parent) {
    void* lib = dlopen("libSoulverWrapper.so", RTLD_LAZY);
    if (!lib) {
        qWarning() << "Soulver: unable to load libSoulverWrapper.so:" << dlerror();
        return;
    }

    const auto init = reinterpret_cast<bool (*)(const char*)>(dlsym(lib, "soulver_initialize"));
    const auto evaluate = reinterpret_cast<char* (*)(const char*)>(dlsym(lib, "soulver_evaluate"));
    const auto resources = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation, u"soulver-core/resources"_s, QStandardPaths::LocateDirectory);

    if (!init || !evaluate || resources.isEmpty() || !init(resources.toUtf8().constData())) {
        qWarning() << "Soulver: unable to initialise SoulverCore";
        return;
    }

    free(evaluate("1+1")); // The first evaluation is slow, so get it out of the way
    m_evaluate = evaluate;
}

QVariant Soulver::eval(const QString& expr) const {
    if (!m_evaluate || expr.trimmed().isEmpty())
        return {};

    char* answer = m_evaluate(expr.toUtf8().constData());
    const auto json = QJsonDocument::fromJson(answer).object();
    free(answer);

    const auto value = json.value(u"value"_s).toString();
    const auto type = json.value(u"type"_s).toString();
    if (value.isEmpty() || type == u"none"_s)
        return {};

    return QVariantMap{ { u"value"_s, value }, { u"type"_s, type } };
}

} // namespace caelestia
