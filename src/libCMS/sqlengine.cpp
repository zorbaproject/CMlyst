#include "sqlengine.h"
#include "page.h"
#include "menu.h"

#include <Cutelyst/Plugins/View/Grantlee/grantleeview.h>
#include <Cutelyst/Plugins/Utils/Sql>
#include <Cutelyst/Context>
#include <Cutelyst/Application>

#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <QDebug>

using namespace CMS;

SqlEngine::SqlEngine(QObject *parent) : Engine(parent)
{

}

bool SqlEngine::init(const QHash<QString, QString> &settings)
{
    QString root = settings.value(QStringLiteral("root"));
    if (root.isEmpty()) {
        root = QDir::currentPath();
    }

    const QString dbPath = root + QLatin1String("/cmlyst.sqlite");
    bool create = !QFile::exists(dbPath);

    if (QSqlDatabase::contains(QStringLiteral("cmlyst"))) {
        return true;
    }

    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), Cutelyst::Sql::databaseNameThread(QStringLiteral("cmlyst")));
    db.setDatabaseName(dbPath);
    if (db.open()) {
        qDebug() << "Database is open:" << dbPath << db.connectionName();
        if (create) {
            createDb();
            qDebug() << "Database tables created";
        }
    } else {
        qCritical() << "Error opening database" << dbPath << db.lastError().databaseText();
        return false;
    }

    return true;
}

Page *SqlEngine::createPageObj(const QSqlQuery &query, QObject *parent)
{
    auto page = new Page(parent);
    page->setAllowComments(query.value(QStringLiteral("allow_comments")).toBool());

    int author_id = query.value(QStringLiteral("author_id")).toInt();
    Author author = m_usersId.value(author_id);
    page->setAuthor(author);
    page->setPage(query.value(QStringLiteral("page")).toBool());
    page->setContent(query.value(QStringLiteral("content")).toString(), true);

    QDateTime updated = QDateTime::fromString(query.value(QStringLiteral("updated_at")).toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    updated.setTimeSpec(Qt::UTC);
    page->setUpdated(updated.toTimeZone(m_timezone));

    QDateTime created = QDateTime::fromString(query.value(QStringLiteral("created_at")).toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    created.setTimeSpec(Qt::UTC);
    page->setCreated(created.toTimeZone(m_timezone));

    QDateTime published = QDateTime::fromString(query.value(QStringLiteral("published_at")).toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    published.setTimeSpec(Qt::UTC);
    page->setPublished(published.toTimeZone(m_timezone));

    page->setName(query.value(QStringLiteral("title")).toString());
    page->setPath(query.value(QStringLiteral("path")).toString());
    page->setUuid(query.value(QStringLiteral("uuid")).toString());
    page->setId(query.value(QStringLiteral("id")).toInt());
    return page;
}

Page *SqlEngine::getPage(const QString &path, QObject *parent)
{
    QSqlQuery query = CPreparedSqlQueryThreadForDB(QStringLiteral("SELECT id, uuid, path, title, author_id, content,"
                                                                  " created_at, updated_at, published_at, page, allow_comments "
                                                                  "FROM posts "
                                                                  "WHERE path = :path"),
                                                   QStringLiteral("cmlyst"));
    if (!path.isNull()) {
        query.bindValue(QStringLiteral(":path"), path);
    } else {
        query.bindValue(QStringLiteral(":path"), QStringLiteral(""));
    }

    if (Q_LIKELY(query.exec())) {
        if (query.next()) {
            return createPageObj(query, parent);
        }
    } else {
        qWarning() << "Failed to get page" << path << query.lastError().databaseText();
    }
    return 0;
}

bool SqlEngine::removePage(int id)
{
    QSqlQuery query = CPreparedSqlQueryThreadForDB(QStringLiteral("DELETE FROM posts "
                                                                  "WHERE id = :id"),
                                                   QStringLiteral("cmlyst"));
    query.bindValue(QStringLiteral(":id"), id);
    if (query.exec() && query.numRowsAffected() == 1) {
        return true;
    } else {
        qWarning() << "Failed to remove page" << id << query.lastError().databaseText() << "numRowsAffected" << query.numRowsAffected();
        return false;
    }
}

QString sortString(Engine::SortFlags sort)
{
    if (sort & Engine::Reversed) {
        if (sort & Engine::Name) {
            return QStringLiteral(" ORDER BY name ASC ");
        } else if (sort & Engine::Date) {
            return QStringLiteral(" ORDER BY date ASC ");
        }
    } else {
        if (sort & Engine::Name) {
            return QStringLiteral(" ORDER BY name DESC ");
        } else if (sort & Engine::Date) {
            return QStringLiteral(" ORDER BY date DESC ");
        }
    }

    return QString();
}

QList<Page *> SqlEngine::listPages(QObject *parent, Engine::Filters filters, Engine::SortFlags sort, int depth, int limit)
{
    QList<Page *> ret;
    QSqlQuery query;
    if (filters == Engine::Pages) {
        query = CPreparedSqlQueryThreadForDB(QStringLiteral("SELECT id, uuid, path, title, author_id, content,"
                                                            " created_at, updated_at, published_at, page, allow_comments "
                                                            "FROM posts "
                                                            "WHERE page = 1 "
                                                            "ORDER BY created_at DESC "
                                                            "LIMIT :limit "
                                                            ),
                                             QStringLiteral("cmlyst"));
    } else if (filters == Engine::Posts) {
        query = CPreparedSqlQueryThreadForDB(QStringLiteral("SELECT id, uuid, path, title, author_id, content,"
                                                            " created_at, updated_at, published_at, page, allow_comments "
                                                            "FROM posts "
                                                            "WHERE page = 0 "
                                                            "ORDER BY created_at DESC "
                                                            "LIMIT :limit "
                                                            ),
                                             QStringLiteral("cmlyst"));
    } else {
        query = CPreparedSqlQueryThreadForDB(QStringLiteral("SELECT id, uuid, path, title, author_id, content,"
                                                            " created_at, updated_at, published_at, page, allow_comments "
                                                            "FROM posts "
                                                            "ORDER BY created_at DESC "
                                                            "LIMIT :limit "
                                                            ),
                                             QStringLiteral("cmlyst"));
    }

    query.bindValue(QStringLiteral(":limit"), limit);
    if (query.exec()) {
        while (query.next()) {
            ret.append(createPageObj(query, parent));
        }
    }
    return ret;
}

QHash<QString, QString> SqlEngine::settings() const
{
    return m_settings;
}

QString SqlEngine::settingsValue(const QString &key, const QString &defaultValue) const
{
    return m_settings.value(key, defaultValue);
}

bool SqlEngine::setSettingsValue(Cutelyst::Context *c, const QString &key, const QString &value)
{
    static QSqlDatabase db = QSqlDatabase::database(Cutelyst::Sql::databaseNameThread(QStringLiteral("cmlyst")));
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery query = CPreparedSqlQueryThreadForDB(QStringLiteral("INSERT OR REPLACE INTO settings "
                                                                  "(key, value) "
                                                                  "VALUES "
                                                                  "(:key, :value)"),
                                                   QStringLiteral("cmlyst"));

    if (key != QLatin1String("modified")) {
        query.bindValue(QStringLiteral(":key"), key);
        query.bindValue(QStringLiteral(":value"), value);
        if (!query.exec()) {
            qWarning() << "Failed to save settings" << query.lastError().databaseText();
            db.rollback();
            return false;
        }
    }

    qint64 currentDateTime = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() / 1000;
    query.bindValue(QStringLiteral(":key"), QStringLiteral("modified"));
    query.bindValue(QStringLiteral(":value"), currentDateTime);
    if (!query.exec()) {
        qWarning() << "Failed to save settings" << query.lastError().databaseText();
        db.rollback();
        return false;
    }

    if (db.commit()) {
        m_settingsDate = -1;
        m_settingsDateTime = QDateTime();
        c->setProperty("_sql_engine_date", QVariant());
        loadSettings(c);

        return true;
    }
    return false;
}

QList<Menu *> SqlEngine::menus()
{
    return m_menus;
}

bool SqlEngine::saveMenu(Cutelyst::Context *c, Menu *menu, bool replace)
{
    QJsonObject menusObj;
    auto menus = m_menus;

    if (menu) {
        for (const auto menuIt : menus) {
            if (menuIt->id() == menu->id()) {
                menus.removeOne(menuIt);
                break;
            }
        }
        menus.push_back(menu);
    }

    for (CMS::Menu *menu : menus) {
        QJsonObject objMenu;
//        objMenu.insert(QStringLiteral("id"), menu->id());
        objMenu.insert(QStringLiteral("name"), menu->name());

        QJsonArray locations;
        const auto menuLocations = menu->locations();
        for (const auto location : menuLocations) {
            locations.append(location);
        }
        objMenu.insert(QStringLiteral("locations"), locations);

        QJsonArray entriesJson;
        const QList<QVariantHash> entries = menu->entries();
        for (const auto entry : entries) {
            entriesJson.append(QJsonObject::fromVariantHash(entry));
        }
        objMenu.insert(QStringLiteral("entries"), entriesJson);

        menusObj.insert(menu->id(), objMenu);
    }

    QJsonDocument doc(menusObj);
    setSettingsValue(c, QStringLiteral("menus"), QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

bool SqlEngine::removeMenu(Cutelyst::Context *c, const QString &name)
{
    for (const auto menuIt : m_menus) {
        if (menuIt->id() == name) {
            m_menus.removeOne(menuIt);
            saveMenu(c, nullptr, false);
            break;
        }
    }
}

QHash<QString, Menu *> SqlEngine::menuLocations()
{
    return m_menuLocations;
}

bool SqlEngine::settingsIsWritable() const
{
    return true;
}

QHash<QString, QString> SqlEngine::loadSettings(Cutelyst::Context *c)
{
    QVariant loadedDate = c->property("_sql_engine_date");
    if (loadedDate.isNull()) {
        QSqlQuery query = CPreparedSqlQueryThreadForDB(QStringLiteral("SELECT value FROM settings WHERE key = 'modified'"),
                                                       QStringLiteral("cmlyst"));
        if (query.exec() && query.next()) {
            loadedDate = query.value(0).toLongLong();
            c->setProperty("_sql_engine_date", loadedDate);
        }

        qint64 settingsDate = loadedDate.toLongLong();
        if (settingsDate != m_settingsDate) {
            m_settingsDate = settingsDate;
            m_settingsDateTime = QDateTime::fromMSecsSinceEpoch(settingsDate * 1000);
            m_settings.clear();

            QSqlQuery query = CPreparedSqlQueryThreadForDB(QStringLiteral("SELECT key, value FROM settings"),
                                                           QStringLiteral("cmlyst"));
            if (query.exec()) {
                while (query.next()) {
                    m_settings.insert(query.value(0).toString(), query.value(1).toString());
                }
            }

            const QString tz = m_settings.value(QStringLiteral("timezone"));
            if (!tz.isEmpty()) {
                m_timezone = QTimeZone(tz.toUtf8());
            }

            if (!m_timezone.isValid()) {
                m_timezone = QTimeZone::systemTimeZone();
            }

            loadMenus();
            loadUsers();

            configureView(c);
        }
    }

    return m_settings;
}

QDateTime SqlEngine::lastModified()
{
    return m_settingsDateTime;
}

QVariantList SqlEngine::users()
{
    return m_users;
}

QHash<QString, QString> SqlEngine::user(const QString &slug)
{
    return m_usersSlug.value(slug);
}

QHash<QString, QString> SqlEngine::user(int id)
{
    return m_usersId.value(id);
}

bool SqlEngine::savePageBackend(Page *page)
{
    qDebug() << Q_FUNC_INFO << page->path() << page->id() << page->uuid();
    QSqlQuery query = CPreparedSqlQueryThreadForDB(QStringLiteral("INSERT OR REPLACE INTO posts "
                                                                  "(path, uuid, title, author_id, content, html,"
                                                                  " created_at, updated_at, published_at, page, published, allow_comments) "
                                                                  "VALUES "
                                                                  "(:path, :uuid, :title, :author_id, :content, :html,"
                                                                  " :created_at, :updated_at, :published_at, :page, :published, :allow_comments)"),
                                                   QStringLiteral("cmlyst"));
    query.bindValue(QStringLiteral(":path"), page->path());
    query.bindValue(QStringLiteral(":uuid"), page->uuid());
    query.bindValue(QStringLiteral(":title"), page->name());
    query.bindValue(QStringLiteral(":author_id"), page->author().value(QStringLiteral("id")).toInt());
    query.bindValue(QStringLiteral(":content"), page->content().get());
    query.bindValue(QStringLiteral(":html"), page->content().get());
    query.bindValue(QStringLiteral(":created_at"), page->created().toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    query.bindValue(QStringLiteral(":updated_at"), page->updated().toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    query.bindValue(QStringLiteral(":published_at"), page->published().toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    query.bindValue(QStringLiteral(":page"), page->page());
    query.bindValue(QStringLiteral(":published"), true);
    query.bindValue(QStringLiteral(":allow_comments"), page->allowComments());
    if (!query.exec()) {
        qWarning() << "Failed to save page" << query.lastError().databaseText();
        return false;
    }
    return true;
}

void SqlEngine::loadMenus()
{
    QList<CMS::Menu *> menus;
    QHash<QString, CMS::Menu *> menuLocations;

    const QString menusSetting = settingsValue(QStringLiteral("menus"));
    QJsonDocument doc = QJsonDocument::fromJson(menusSetting.toUtf8());
    const QJsonObject menusObj = doc.object();
    qDebug() << Q_FUNC_INFO << menusObj;
    auto it = menusObj.constBegin();
    while (it != menusObj.constEnd()) {
//    for (const QJsonValue &value : menusObj) {
//        qDebug() << Q_FUNC_INFO << value;
        QJsonObject obj = it.value().toObject();
//        Menu *menu = new Menu(obj.value(QStringLiteral("id")).toString());
        Menu *menu = new Menu(it.key());

        menu->setName(obj.value(QStringLiteral("name")).toString());
        menu->setAutoAddPages(obj.value(QStringLiteral("autoAddPages")).toBool());

        QList<QVariantHash> urls;
        const QJsonArray urlsJson = obj.value(QStringLiteral("entries")).toArray();
        for (const QJsonValue &url : urlsJson) {
            urls.append(url.toObject().toVariantHash());
        }
        menu->setEntries(urls);

        QStringList locations;
        const QJsonArray locationsJson = obj.value(QStringLiteral("locations")).toArray();
        for (const QJsonValue &location : locationsJson) {
            locations.append(location.toString());
        }
        menu->setLocations(locations);

        bool added = false;
        Q_FOREACH (const QString &location, locations) {
            if (!menuLocations.contains(location)) {
                menuLocations.insert(location, menu);
                added = true;
            }
        }
        qDebug() << "MENU";
        qDebug() << menu->id() << menu->name() << menu->entries() << menu->locations() << menu->autoAddPages();

        menus.push_back(menu);

        ++it;
    }
    qDebug() << "MENUS" << menus;

    qDeleteAll(m_menus);

    m_menus = menus;
    qDebug() << "MENUS" << menuLocations;
    m_menuLocations = menuLocations;
}

void SqlEngine::loadUsers()
{
    m_users.clear();
    m_usersSlug.clear();
    m_usersId.clear();
    QSqlQuery query = CPreparedSqlQueryThreadForDB(QStringLiteral("SELECT id, slug, email, json "
                                                                  "FROM users "),
                                                   QStringLiteral("cmlyst"));
    if (Q_LIKELY(query.exec())) {
        if (query.next()) {
            QHash<QString, QString> user;

            const QString id = query.value(0).toString();
            user.insert(QStringLiteral("id"), id);
            const QString slug = query.value(1).toString();
            user.insert(QStringLiteral("slug"), slug);
            user.insert(QStringLiteral("email"), query.value(2).toString());

            QJsonDocument doc = QJsonDocument::fromJson(query.value(3).toString().toUtf8());
            QJsonObject obj = doc.object();

            const QStringList fields = {
                QStringLiteral("name"),
                QStringLiteral("bio"),
                QStringLiteral("location"),
                QStringLiteral("website"),
                QStringLiteral("twitter"),
                QStringLiteral("facebook"),
                QStringLiteral("image"),
                QStringLiteral("cover"),
                QStringLiteral("url"),
            };

            for (const QString &field : fields) {
                user.insert(field, obj.value(field).toString());
            }

            m_users.push_back(QVariant::fromValue(user));
            m_usersSlug.insert(slug, user);
            m_usersId.insert(id.toInt(), user);

            qDebug() << "USER " << user;
        }
    }
    qDebug() << "USERS " << m_users.size();
}

void SqlEngine::configureView(Cutelyst::Context *c)
{
    const QString theme = m_settings.value(QStringLiteral("theme"), QStringLiteral("default"));

    if (m_theme != theme) {
        m_theme = theme;

        Cutelyst::Application *app = c->app();

        auto view = qobject_cast<Cutelyst::GrantleeView*>(app->view());

        const QDir themeDir = app->pathTo({ QStringLiteral("root"), QStringLiteral("themes") });

        view->setIncludePaths({ themeDir.absoluteFilePath(theme) });
    }
}

void SqlEngine::createDb()
{
    QSqlQuery query(QSqlDatabase::database(Cutelyst::Sql::databaseNameThread(QStringLiteral("cmlyst"))));
    qDebug() << "createDb";

    bool ret = query.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    qDebug() << "PRAGMA journal_mode = WAL" << ret << query.lastError().databaseText();

    if (!query.exec(QStringLiteral("CREATE TABLE posts "
                                   "( id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT"
                                   ", uuid TEXT NOT NULL UNIQUE "
                                   ", path TEXT NOT NULL UNIQUE "
                                   ", title TEXT "
                                   ", content TEXT "
                                   ", html TEXT "
                                   ", language TEXT "
                                   ", status TEXT "
                                   ", meta_title TEXT "
                                   ", meta_description TEXT "
                                   ", page BOOL NOT NULL "
                                   ", published BOOL NOT NULL "
                                   ", allow_comments BOOL NOT NULL "
                                   ", author_id INTEGER "
                                   ", created_at datetime NOT NULL "
                                   ", created_by INTEGER "
                                   ", updated_at datetime "
                                   ", updated_by INTEGER "
                                   ", published_at datetime "
                                   ", published_by INTEGER "
                                   ")"))) {
        qCritical() << "Error creating database" << query.lastError().text();
        exit(1);
    }

    if (!query.exec(QStringLiteral("CREATE TABLE settings "
                                   "( key TEXT NOT NULL PRIMARY KEY "
                                   ", value TEXT"
                                   ")"))) {
        qCritical() << "Error creating database" << query.lastError().text();
        exit(1);
    }

    if (!query.exec(QStringLiteral("CREATE TABLE users "
                                   "( id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT "
                                   ", slug TEXT NOT NULL UNIQUE "
                                   ", email TEXT NOT NULL UNIQUE "
                                   ", password TEXT NOT NULL "
                                   ", json TEXT "
                                   ")"))) {
        qCritical() << "Error creating database" << query.lastError().text();
        exit(1);
    }
}
