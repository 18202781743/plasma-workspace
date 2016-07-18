/*
 *   Copyright 2009 Aaron Seigo <aseigo@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "scriptengine.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QScriptValueIterator>
#include <QStandardPaths>
#include <QFutureWatcher>

#include <QDebug>
#include <klocalizedstring.h>
#include <kmimetypetrader.h>
#include <kservicetypetrader.h>
#include <kshell.h>

// KIO
//#include <kemailsettings.h> // no camelcase include

#include <Plasma/Applet>
#include <Plasma/PluginLoader>
#include <Plasma/Containment>
#include <qstandardpaths.h>
#include <KPackage/Package>
#include <KPackage/PackageLoader>

#include "appinterface.h"
#include "containment.h"
#include "configgroup.h"
#include "i18n.h"
#include "panel.h"
#include "widget.h"
#include "../shellcorona.h"
#include "../standaloneappcorona.h"

QScriptValue constructQRectFClass(QScriptEngine *engine);

namespace {
    template <typename T>
    inline void awaitFuture(const QFuture<T> &future)
    {
        while (!future.isFinished()) {
            QCoreApplication::processEvents();
        }
    }
}

namespace WorkspaceScripting
{

ScriptEngine::ScriptEngine(Plasma::Corona *corona, QObject *parent)
    : QScriptEngine(parent),
      m_corona(corona)
{
    Q_ASSERT(m_corona);
    AppInterface *interface = new AppInterface(this);
    connect(interface, &AppInterface::print, this, &ScriptEngine::print);
    m_scriptSelf = newQObject(interface, QScriptEngine::QtOwnership,
                              QScriptEngine::ExcludeSuperClassProperties |
                              QScriptEngine::ExcludeSuperClassMethods);
    setupEngine();
    connect(this, &ScriptEngine::signalHandlerException, this, &ScriptEngine::exception);
    bindI18N(this);
}

ScriptEngine::~ScriptEngine()
{
}

QScriptValue ScriptEngine::desktopById(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() == 0) {
        return context->throwError(i18n("activityById requires an id"));
    }

    const uint id = context->argument(0).toInt32();
    ScriptEngine *env = envFor(engine);
    foreach (Plasma::Containment *c, env->m_corona->containments()) {
        if (c->id() == id && !isPanel(c)) {
            return env->wrap(c);
        }
    }

    return engine->undefinedValue();
}

QStringList ScriptEngine::availableActivities(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)

    ScriptEngine *env = envFor(engine);

    ShellCorona *sc = qobject_cast<ShellCorona *>(env->m_corona);
    StandaloneAppCorona *ac = qobject_cast<StandaloneAppCorona *>(env->m_corona);
    if (sc) {
        return sc->availableActivities();
    } else if (ac) {
        return ac->availableActivities();
    }

    return QStringList();
}

QScriptValue ScriptEngine::desktopsForActivity(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() == 0) {
        return context->throwError(i18n("desktopsForActivity requires an id"));
    }

    QScriptValue containments = engine->newArray();
    int count = 0;

    const QString id = context->argument(0).toString();

    // confirm this activity actually exists
    bool found = false;
    for (const QString &act: availableActivities(context, engine)) {
        if (act == id) {
            found = true;
            break;
        }
    }

    if (!found) {
        containments.setProperty(QStringLiteral("length"), 0);
        return containments;
    }

    ScriptEngine *env = envFor(engine);
    foreach (Plasma::Containment *c, env->m_corona->containments()) {
        if (c->activity() == id && !isPanel(c)) {
            containments.setProperty(count, env->wrap(c));
            ++count;
        }
    }

    if (count == 0) {
        // we have no desktops for this activity, so lets make them now
        // this can happen when the activity already exists but has never been activated
        // with the current shell package and layout.js is run to set up the shell for the
        // first time
        const int numScreens = env->m_corona->numScreens();
        for (int i = 0; i < numScreens; ++i) {
            ShellCorona *sc = qobject_cast<ShellCorona *>(env->m_corona);
            StandaloneAppCorona *ac = qobject_cast<StandaloneAppCorona *>(env->m_corona);
            if (sc) {
                Plasma::Containment *c = sc->createContainmentForActivity(id, i);
                containments.setProperty(count, env->wrap(c));
                ++count;
            } else if (ac) {
                Plasma::Containment *c = ac->createContainmentForActivity(id, i);
                containments.setProperty(count, env->wrap(c));
                ++count;
            }
        }
    }

    containments.setProperty(QStringLiteral("length"), count);
    return containments;
}

QScriptValue ScriptEngine::desktopForScreen(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() == 0) {
        return context->throwError(i18n("activityForScreen requires a screen id"));
    }

    const uint screen = context->argument(0).toInt32();
    ScriptEngine *env = envFor(engine);
    return env->wrap(env->m_corona->containmentForScreen(screen));
}

QScriptValue ScriptEngine::createActivity(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() < 0) {
        return context->throwError(i18n("createActivity required the activity name"));
    }

    const QString name = context->argument(0).toString();
    QString plugin = context->argument(1).toString();

    ScriptEngine *env = envFor(engine);

    KActivities::Controller controller;

    // This is not the nicest way to do this, but createActivity
    // is a synchronous API :/
    QFuture<QString> futureId = controller.addActivity(name);
    awaitFuture(futureId);

    QString id = futureId.result();

    qDebug() << "Setting default Containment plugin:" << plugin;

    if (plugin.isEmpty() || plugin == QLatin1String("undefined")) {
        KConfigGroup shellCfg = KConfigGroup(KSharedConfig::openConfig(env->m_corona->package().filePath("defaults")), "Desktop");
        plugin = shellCfg.readEntry("Containment", "org.kde.desktopcontainment");
    }

    ShellCorona *sc = qobject_cast<ShellCorona *>(env->m_corona);
    StandaloneAppCorona *ac = qobject_cast<StandaloneAppCorona *>(env->m_corona);
    if (sc) {
        sc->insertActivity(id, plugin);
    } else if (ac) {
        ac->insertActivity(id, plugin);
    }

    return QScriptValue(id);
}

QScriptValue ScriptEngine::setCurrentActivity(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)

    if (context->argumentCount() < 0) {
        return context->throwError(i18n("setCurrentActivity required the activity id"));
    }

    const QString id = context->argument(0).toString();

    KActivities::Controller controller;

    QFuture<bool> task = controller.setCurrentActivity(id);
    awaitFuture(task);

    return QScriptValue(task.result());
}

QScriptValue ScriptEngine::setActivityName(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)

    if (context->argumentCount() < 2) {
        return context->throwError(i18n("setActivityName required the activity id and name"));
    }

    const QString id = context->argument(0).toString();
    const QString name = context->argument(1).toString();

    KActivities::Controller controller;

    QFuture<void> task = controller.setActivityName(id, name);
    awaitFuture(task);

    return QScriptValue();
}

QScriptValue ScriptEngine::activityName(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)

    if (context->argumentCount() < 1) {
        return context->throwError(i18n("setActivityName required the activity id and name"));
    }

    const QString id = context->argument(0).toString();

    KActivities::Info info(id);

    return QScriptValue(info.name());
}

QScriptValue ScriptEngine::currentActivity(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)
    Q_UNUSED(context)

    KActivities::Consumer consumer;
    return consumer.currentActivity();
}

QScriptValue ScriptEngine::activities(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context)

    return qScriptValueFromSequence(engine, availableActivities(context, engine));
}

QScriptValue ScriptEngine::newPanel(QScriptContext *context, QScriptEngine *engine)
{
    QString plugin(QStringLiteral("org.kde.panel"));

    if (context->argumentCount() > 0) {
        plugin = context->argument(0).toString();
    }

    return createContainment(QStringLiteral("Panel"), plugin, context, engine);
}

QScriptValue ScriptEngine::createContainment(const QString &type, const QString &defaultPlugin,
                                             QScriptContext *context, QScriptEngine *engine)
{
    QString plugin = context->argumentCount() > 0 ? context->argument(0).toString() :
                                                    defaultPlugin;

    bool exists = false;
    const KPluginInfo::List list = Plasma::PluginLoader::listContainmentsOfType(type);
    foreach (const KPluginInfo &info, list) {
        if (info.pluginName() == plugin) {
            exists = true;
            break;
        }
    }

    if (!exists) {
        return context->throwError(i18n("Could not find a plugin for %1 named %2.", type, plugin));
    }


    ScriptEngine *env = envFor(engine);
    Plasma::Containment *c = 0;
    if (type == QLatin1String("Panel")) {
        ShellCorona *sc = qobject_cast<ShellCorona *>(env->m_corona);
        StandaloneAppCorona *ac = qobject_cast<StandaloneAppCorona *>(env->m_corona);
        if (sc) {
            c = sc->addPanel(plugin);
        } else if (ac) {
            c = ac->addPanel(plugin);
        }
    } else {
        c = env->m_corona->createContainment(plugin);
    }

    if (c) {
        if (type == QLatin1String("Panel")) {
            // some defaults
            c->setFormFactor(Plasma::Types::Horizontal);
            c->setLocation(Plasma::Types::TopEdge);
        }
        c->updateConstraints(Plasma::Types::AllConstraints | Plasma::Types::StartupCompletedConstraint);
        c->flushPendingConstraintsEvents();
    }

    return env->wrap(c);
}

QScriptValue ScriptEngine::wrap(Plasma::Applet *w)
{
    Widget *wrapper = new Widget(w);
    QScriptValue v = newQObject(wrapper, QScriptEngine::ScriptOwnership,
                                QScriptEngine::ExcludeSuperClassProperties |
                                QScriptEngine::ExcludeSuperClassMethods);
    return v;
}

QScriptValue ScriptEngine::wrap(Plasma::Containment *c)
{
    Containment *wrapper = isPanel(c) ? new Panel(c) : new Containment(c);
    return wrap(wrapper);
}

QScriptValue ScriptEngine::wrap(Containment *c)
{
    QScriptValue v = newQObject(c, QScriptEngine::ScriptOwnership,
                                QScriptEngine::ExcludeSuperClassProperties |
                                QScriptEngine::ExcludeSuperClassMethods);
    v.setProperty(QStringLiteral("widgetById"), newFunction(Containment::widgetById));
    v.setProperty(QStringLiteral("addWidget"), newFunction(Containment::addWidget));
    v.setProperty(QStringLiteral("widgets"), newFunction(Containment::widgets));

    return v;
}

int ScriptEngine::defaultPanelScreen() const
{
    return 0;
}

ScriptEngine *ScriptEngine::envFor(QScriptEngine *engine)
{
    QObject *object = engine->globalObject().toQObject();
    Q_ASSERT(object);

    AppInterface *interface = qobject_cast<AppInterface *>(object);
    Q_ASSERT(interface);

    ScriptEngine *env = qobject_cast<ScriptEngine *>(interface->parent());
    Q_ASSERT(env);

    return env;
}

QScriptValue ScriptEngine::panelById(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() == 0) {
        return context->throwError(i18n("panelById requires an id"));
    }

    const uint id = context->argument(0).toInt32();
    ScriptEngine *env = envFor(engine);
    foreach (Plasma::Containment *c, env->m_corona->containments()) {
        if (c->id() == id && isPanel(c)) {
            return env->wrap(c);
        }
    }

    return engine->undefinedValue();
}

QScriptValue ScriptEngine::panels(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context)

    QScriptValue panels = engine->newArray();
    ScriptEngine *env = envFor(engine);
    int count = 0;

    foreach (Plasma::Containment *c, env->m_corona->containments()) {
        if (isPanel(c)) {
            panels.setProperty(count, env->wrap(c));
            ++count;
        }
    }

    panels.setProperty(QStringLiteral("length"), count);
    return panels;
}

QScriptValue ScriptEngine::fileExists(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)
    if (context->argumentCount() == 0) {
        return false;
    }

    const QString path = context->argument(0).toString();
    if (path.isEmpty()) {
        return false;
    }

    QFile f(KShell::tildeExpand(path));
    return f.exists();
}

QScriptValue ScriptEngine::loadTemplate(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)
    if (context->argumentCount() == 0) {
        // qDebug() << "no arguments";
        return false;
    }

    const QString layout = context->argument(0).toString();
    if (layout.isEmpty() || layout.contains(QStringLiteral("'"))) {
        // qDebug() << "layout is empty";
        return false;
    }

    const QString constraint = QStringLiteral("[X-KDE-PluginInfo-Name] == '%2'").arg(layout);
    KService::List offers = KServiceTypeTrader::self()->query(QStringLiteral("Plasma/LayoutTemplate"), constraint);

    if (offers.isEmpty()) {
        // qDebug() << "offers fail" << constraint;
        return false;
    }

    KPackage::Package package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LayoutTemplate"));
    KPluginInfo info(offers.first());

    const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, package.defaultPackageRoot() + info.pluginName() + "/metadata.desktop");
    if (path.isEmpty()) {
        // qDebug() << "script path is empty";
        return false;
    }

    package.setPath(info.pluginName());

    const QString scriptFile = package.filePath("mainscript");
    if (scriptFile.isEmpty()) {
        // qDebug() << "scriptfile is empty";
        return false;
    }

    QFile file(scriptFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << i18n("Unable to load script file: %1", path);
        return false;
    }

    QString script = file.readAll();
    if (script.isEmpty()) {
        // qDebug() << "script is empty";
        return false;
    }

    ScriptEngine *env = envFor(engine);
    env->globalObject().setProperty(QStringLiteral("templateName"), env->newVariant(info.name()), QScriptValue::ReadOnly | QScriptValue::Undeletable);
    env->globalObject().setProperty(QStringLiteral("templateComment"), env->newVariant(info.comment()), QScriptValue::ReadOnly | QScriptValue::Undeletable);

    QScriptValue rv = env->newObject();
    QScriptContext *ctx = env->pushContext();
    ctx->setThisObject(rv);

    env->evaluateScript(script, path);

    env->popContext();
    return rv;
}

QScriptValue ScriptEngine::applicationExists(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)
    if (context->argumentCount() == 0) {
        return false;
    }

    const QString application = context->argument(0).toString();
    if (application.isEmpty()) {
        return false;
    }

    // first, check for it in $PATH
    if (!QStandardPaths::findExecutable(application).isEmpty()) {
        return true;
    }

    if (KService::serviceByStorageId(application)) {
        return true;
    }

    if (application.contains(QStringLiteral("'"))) {
        // apostrophes just screw up the trader lookups below, so check for it
        return false;
    }

    // next, consult ksycoca for an app by that name
    if (!KServiceTypeTrader::self()->query(QStringLiteral("Application"), QStringLiteral("Name =~ '%1'").arg(application)).isEmpty()) {
        return true;
    }

    // next, consult ksycoca for an app by that generic name
    if (!KServiceTypeTrader::self()->query(QStringLiteral("Application"), QStringLiteral("GenericName =~ '%1'").arg(application)).isEmpty()) {
        return true;
    }

    return false;
}

QString ScriptEngine::onlyExec(const QString &commandLine)
{
    if (commandLine.isEmpty()) {
        return commandLine;
    }

    return KShell::splitArgs(commandLine, KShell::TildeExpand).first();
}

QScriptValue ScriptEngine::defaultApplication(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)
    if (context->argumentCount() == 0) {
        return false;
    }

    const QString application = context->argument(0).toString();
    if (application.isEmpty()) {
        return false;
    }

    const bool storageId = context->argumentCount() < 2 ? false : context->argument(1).toBool();

    // FIXME: there are some pretty horrible hacks below, in the sense that they assume a very
    // specific implementation system. there is much room for improvement here. see
    // kdebase-runtime/kcontrol/componentchooser/ for all the gory details ;)
    if (application.compare(QLatin1String("mailer"), Qt::CaseInsensitive) == 0) {
  //      KEMailSettings settings;

        // in KToolInvocation, the default is kmail; but let's be friendlier :)
//        QString command = settings.getSetting(KEMailSettings::ClientProgram);
        QString command;
        if (command.isEmpty()) {
            if (KService::Ptr kontact = KService::serviceByStorageId(QStringLiteral("kontact"))) {
                return storageId ? kontact->storageId() : onlyExec(kontact->exec());
            } else if (KService::Ptr kmail = KService::serviceByStorageId(QStringLiteral("kmail"))) {
                return storageId ? kmail->storageId() : onlyExec(kmail->exec());
            }
        }

        if (!command.isEmpty()) {
            //if (settings.getSetting(KEMailSettings::ClientTerminal) == "true") {
        if (false) {
                KConfigGroup confGroup(KSharedConfig::openConfig(), "General");
                const QString preferredTerminal = confGroup.readPathEntry("TerminalApplication", QStringLiteral("konsole"));
                command = preferredTerminal + QLatin1String(" -e ") + command;
            }

            return command;
        }
    } else if (application.compare(QLatin1String("browser"), Qt::CaseInsensitive) == 0) {
        KConfigGroup config(KSharedConfig::openConfig(), "General");
        QString browserApp = config.readPathEntry("BrowserApplication", QString());
        if (browserApp.isEmpty()) {
            const KService::Ptr htmlApp = KMimeTypeTrader::self()->preferredService(QStringLiteral("text/html"));
            if (htmlApp) {
                browserApp = storageId ? htmlApp->storageId() : htmlApp->exec();
            }
        } else if (browserApp.startsWith('!')) {
            browserApp = browserApp.mid(1);
        }

        return onlyExec(browserApp);
    } else if (application.compare(QLatin1String("terminal"), Qt::CaseInsensitive) == 0) {
        KConfigGroup confGroup(KSharedConfig::openConfig(), "General");
        return onlyExec(confGroup.readPathEntry("TerminalApplication", QStringLiteral("konsole")));
    } else if (application.compare(QLatin1String("filemanager"), Qt::CaseInsensitive) == 0) {
        KService::Ptr service = KMimeTypeTrader::self()->preferredService(QStringLiteral("inode/directory"));
        if (service) {
            return storageId ? service->storageId() : onlyExec(service->exec());
        }
    } else if (application.compare(QLatin1String("windowmanager"), Qt::CaseInsensitive) == 0) {
        KConfig cfg(QStringLiteral("ksmserverrc"), KConfig::NoGlobals);
        KConfigGroup confGroup(&cfg, "General");
        return onlyExec(confGroup.readEntry("windowManager", QStringLiteral("kwin")));
    } else if (KService::Ptr service = KMimeTypeTrader::self()->preferredService(application)) {
        return storageId ? service->storageId() : onlyExec(service->exec());
    } else {
        // try the files in share/apps/kcm_componentchooser/
        const QStringList services = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kcm_componentchooser/"));
        qDebug() << "ok, trying in" << services;
        foreach (const QString &service, services) {
            if (!service.endsWith(QLatin1String(".desktop"))) {
                continue;
            }
            KConfig config(service, KConfig::SimpleConfig);
            KConfigGroup cg = config.group(QByteArray());
            const QString type = cg.readEntry("valueName", QString());
            //qDebug() << "    checking" << service << type << application;
            if (type.compare(application, Qt::CaseInsensitive) == 0) {
                KConfig store(cg.readPathEntry("storeInFile", QStringLiteral("null")));
                KConfigGroup storeCg(&store, cg.readEntry("valueSection", QString()));
                const QString exec = storeCg.readPathEntry(cg.readEntry("valueName", "kcm_componenchooser_null"),
                                                           cg.readEntry("defaultImplementation", QString()));
                if (!exec.isEmpty()) {
                    return exec;
                }

                break;
            }
        }
    }

    return false;
}

QScriptValue ScriptEngine::applicationPath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)
    if (context->argumentCount() == 0) {
        return false;
    }

    const QString application = context->argument(0).toString();
    if (application.isEmpty()) {
        return false;
    }

    // first, check for it in $PATH
    const QString path = QStandardPaths::findExecutable(application);
    if (!path.isEmpty()) {
        return path;
    }

    if (KService::Ptr service = KService::serviceByStorageId(application)) {
        return QStandardPaths::locate(QStandardPaths::ApplicationsLocation, service->entryPath());
    }

    if (application.contains(QStringLiteral("'"))) {
        // apostrophes just screw up the trader lookups below, so check for it
        return QString();
    }

    // next, consult ksycoca for an app by that name
    KService::List offers = KServiceTypeTrader::self()->query(QStringLiteral("Application"), QStringLiteral("Name =~ '%1'").arg(application));
    if (offers.isEmpty()) {
        // next, consult ksycoca for an app by that generic name
        offers = KServiceTypeTrader::self()->query(QStringLiteral("Application"), QStringLiteral("GenericName =~ '%1'").arg(application));
    }

    if (!offers.isEmpty()) {
        KService::Ptr offer = offers.first();
        return QStandardPaths::locate(QStandardPaths::ApplicationsLocation, offer->entryPath());
    }

    return QString();
}

QScriptValue ScriptEngine::userDataPath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)
    if (context->argumentCount() == 0) {
        return QDir::homePath();
    }

    const QString type = context->argument(0).toString();
    if (type.isEmpty()) {
        return QDir::homePath();
    }

    QStandardPaths::StandardLocation location = QStandardPaths::GenericDataLocation;
    if (type.compare(QLatin1String("desktop"), Qt::CaseInsensitive) == 0) {
        location = QStandardPaths::DesktopLocation;
    } else if (type.compare(QLatin1String("documents"), Qt::CaseInsensitive) == 0) {
        location = QStandardPaths::DocumentsLocation;
    } else if (type.compare(QLatin1String("music"), Qt::CaseInsensitive) == 0) {
        location = QStandardPaths::MusicLocation;
    } else if (type.compare(QLatin1String("video"), Qt::CaseInsensitive) == 0) {
        location = QStandardPaths::MoviesLocation;
    } else if (type.compare(QLatin1String("downloads"), Qt::CaseInsensitive) == 0) {
        location = QStandardPaths::DownloadLocation;
    } else if (type.compare(QLatin1String("pictures"), Qt::CaseInsensitive) == 0) {
        location = QStandardPaths::PicturesLocation;
    } else if (type.compare(QLatin1String("config"), Qt::CaseInsensitive) == 0) {
        location = QStandardPaths::GenericConfigLocation;
    }
    if (context->argumentCount() > 1) {
        QString loc = QStandardPaths::writableLocation(location);
        loc.append(QDir::separator());
        loc.append(context->argument(1).toString());
        return loc;
    }
    const QStringList &locations = QStandardPaths::standardLocations(location);
    return locations.count() ? locations.first() : QString();
}

QScriptValue ScriptEngine::knownWallpaperPlugins(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine)

    QString formFactor;
    if (context->argumentCount() > 0) {
        formFactor = context->argument(0).toString();
    }

    QString constraint;
    if (!formFactor.isEmpty()) {
        constraint.append("[X-Plasma-FormFactors] ~~ '").append(formFactor).append("'");
    }

    QList<KPluginMetaData> wallpapers = KPackage::PackageLoader::self()->listPackages(QStringLiteral("Plasma/Wallpaper"), QString());
    QScriptValue rv = engine->newArray(wallpapers.size());
    for (auto wp : wallpapers) {
        rv.setProperty(wp.name(), engine->newArray(0));
    }

    return rv;
}

QScriptValue ScriptEngine::configFile(QScriptContext *context, QScriptEngine *engine)
{
    ConfigGroup *file = 0;

    if (context->argumentCount() > 0) {
        if (context->argument(0).isString()) {
            file = new ConfigGroup;

            const QString &fileName = context->argument(0).toString();
            const ScriptEngine *env = envFor(engine);
            const Plasma::Corona* corona = env->corona();

            if (fileName == corona->config()->name()) {
                file->setConfig(corona->config());
            } else {
                file->setFile(fileName);
            }

            if (context->argumentCount() > 1) {
                file->setGroup(context->argument(1).toString());
            }
        } else if (ConfigGroup *parent= qobject_cast<ConfigGroup *>(context->argument(0).toQObject())) {
            file = new ConfigGroup(parent);

            if (context->argumentCount() > 1) {
                file->setGroup(context->argument(1).toString());
            }
        }
    } else {
        file = new ConfigGroup;
    }

    QScriptValue v = engine->newQObject(file,
                                        QScriptEngine::ScriptOwnership,
                                        QScriptEngine::ExcludeSuperClassProperties |
                                        QScriptEngine::ExcludeSuperClassMethods);
    return v;

}

void ScriptEngine::setupEngine()
{
    QScriptValue v = globalObject();
    QScriptValueIterator it(v);
    while (it.hasNext()) {
        it.next();
        // we provide our own print implementation, but we want the rest
        if (it.name() != QLatin1String("print")) {
            m_scriptSelf.setProperty(it.name(), it.value());
        }
    }

    m_scriptSelf.setProperty(QStringLiteral("QRectF"), constructQRectFClass(this));
    m_scriptSelf.setProperty(QStringLiteral("createActivity"), newFunction(ScriptEngine::createActivity));
    m_scriptSelf.setProperty(QStringLiteral("setCurrentActivity"), newFunction(ScriptEngine::setCurrentActivity));
    m_scriptSelf.setProperty(QStringLiteral("currentActivity"), newFunction(ScriptEngine::currentActivity));
    m_scriptSelf.setProperty(QStringLiteral("activities"), newFunction(ScriptEngine::activities));
    m_scriptSelf.setProperty(QStringLiteral("setActivityName"), newFunction(ScriptEngine::setActivityName));
    m_scriptSelf.setProperty(QStringLiteral("activityName"), newFunction(ScriptEngine::activityName));
    m_scriptSelf.setProperty(QStringLiteral("setActivityName"), newFunction(ScriptEngine::setActivityName));
    m_scriptSelf.setProperty(QStringLiteral("Panel"), newFunction(ScriptEngine::newPanel, newObject()));
    m_scriptSelf.setProperty(QStringLiteral("desktopsForActivity"), newFunction(ScriptEngine::desktopsForActivity));
    m_scriptSelf.setProperty(QStringLiteral("desktops"), newFunction(ScriptEngine::desktops));
    m_scriptSelf.setProperty(QStringLiteral("desktopById"), newFunction(ScriptEngine::desktopById));
    m_scriptSelf.setProperty(QStringLiteral("desktopForScreen"), newFunction(ScriptEngine::desktopForScreen));
    m_scriptSelf.setProperty(QStringLiteral("panelById"), newFunction(ScriptEngine::panelById));
    m_scriptSelf.setProperty(QStringLiteral("panels"), newFunction(ScriptEngine::panels));
    m_scriptSelf.setProperty(QStringLiteral("fileExists"), newFunction(ScriptEngine::fileExists));
    m_scriptSelf.setProperty(QStringLiteral("loadTemplate"), newFunction(ScriptEngine::loadTemplate));
    m_scriptSelf.setProperty(QStringLiteral("applicationExists"), newFunction(ScriptEngine::applicationExists));
    m_scriptSelf.setProperty(QStringLiteral("defaultApplication"), newFunction(ScriptEngine::defaultApplication));
    m_scriptSelf.setProperty(QStringLiteral("userDataPath"), newFunction(ScriptEngine::userDataPath));
    m_scriptSelf.setProperty(QStringLiteral("applicationPath"), newFunction(ScriptEngine::applicationPath));
    m_scriptSelf.setProperty(QStringLiteral("knownWallpaperPlugins"), newFunction(ScriptEngine::knownWallpaperPlugins));
    m_scriptSelf.setProperty(QStringLiteral("ConfigFile"), newFunction(ScriptEngine::configFile));
    m_scriptSelf.setProperty(QStringLiteral("gridUnit"), ScriptEngine::gridUnit());

    setGlobalObject(m_scriptSelf);
}

bool ScriptEngine::isPanel(const Plasma::Containment *c)
{
    if (!c) {
        return false;
    }

    return c->containmentType() == Plasma::Types::PanelContainment ||
           c->containmentType() == Plasma::Types::CustomPanelContainment;
}

QScriptValue ScriptEngine::desktops(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context)

    QScriptValue containments = engine->newArray();
    ScriptEngine *env = envFor(engine);
    int count = 0;

    foreach (Plasma::Containment *c, env->corona()->containments()) {
        if (!isPanel(c)) {
            containments.setProperty(count, env->wrap(c));
            ++count;
        }
    }

    containments.setProperty(QStringLiteral("length"), count);
    return containments;
}

QScriptValue ScriptEngine::gridUnit()
{
    int gridUnit = QFontMetrics(QGuiApplication::font()).boundingRect(QStringLiteral("M")).height();
    if (gridUnit % 2 != 0) {
        gridUnit++;
    }

    return gridUnit;
}

Plasma::Corona *ScriptEngine::corona() const
{
    return m_corona;
}

bool ScriptEngine::evaluateScript(const QString &script, const QString &path)
{
    //qDebug() << "evaluating" << m_editor->toPlainText();
    evaluate(script, path);
    if (hasUncaughtException()) {
        //qDebug() << "catch the exception!";
        QString error = i18n("Error: %1 at line %2\n\nBacktrace:\n%3",
                             uncaughtException().toString(),
                             QString::number(uncaughtExceptionLineNumber()),
                             uncaughtExceptionBacktrace().join(QStringLiteral("\n  ")));
        emit printError(error);
        return false;
    }

    return true;
}

void ScriptEngine::exception(const QScriptValue &value)
{
    //qDebug() << "exception caught!" << value.toVariant();
    emit printError(value.toVariant().toString());
}

QStringList ScriptEngine::pendingUpdateScripts(Plasma::Corona *corona)
{
    if (!corona->package().metadata().isValid()) {
        qWarning() << "Warning: corona package invalid";
        return QStringList();
    }

    const QString appName = corona->package().metadata().pluginName();
    QStringList scripts;

    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "plasma/shells/" + appName + QStringLiteral("/contents/updates"), QStandardPaths::LocateDirectory);
    Q_FOREACH(const QString& dir, dirs) {
        QDirIterator it(dir, QStringList() << QStringLiteral("*.js"));
        while (it.hasNext()) {
            scripts.append(it.next());
        }
    }
    QStringList scriptPaths;

    if (scripts.isEmpty()) {
        //qDebug() << "no update scripts";
        return scriptPaths;
    }

    KConfigGroup cg(KSharedConfig::openConfig(), "Updates");
    QStringList performed = cg.readEntry("performed", QStringList());
    const QString localXdgDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

    foreach (const QString &script, scripts) {
        if (performed.contains(script)) {
            continue;
        }

        if (script.startsWith(localXdgDir)) {
            // qDebug() << "skipping user local script: " << script;
            continue;
        }

        scriptPaths.append(script);
        performed.append(script);
    }

    cg.writeEntry("performed", performed);
    KSharedConfig::openConfig()->sync();
    return scriptPaths;
}

}



