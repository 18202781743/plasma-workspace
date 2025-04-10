/*
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>

#include <KCModuleData>

namespace NotificationManager
{
class DoNotDisturbSettings;
class NotificationSettings;
class JobSettings;
class BadgeSettings;
class BehaviorSettings;
}

class NotificationsData : public KCModuleData
{
    Q_OBJECT

public:
    explicit NotificationsData(QObject *parent);

    bool isDefaults() const override;

    NotificationManager::DoNotDisturbSettings *dndSettings() const;
    NotificationManager::NotificationSettings *notificationSettings() const;
    NotificationManager::JobSettings *jobSettings() const;
    NotificationManager::BadgeSettings *badgeSettings() const;

    NotificationManager::BehaviorSettings *behaviorSettings(int index) const;
    void insertBehaviorSettings(int index, NotificationManager::BehaviorSettings *settings);
    void loadBehaviorSettings();
    void saveBehaviorSettings();
    void defaultsBehaviorSettings();
    bool isSaveNeededBehaviorSettings() const;
    bool isDefaultsBehaviorSettings() const;

private:
    void readBehaviorSettings();

    NotificationManager::DoNotDisturbSettings *const m_dndSettings;
    NotificationManager::NotificationSettings *const m_notificationSettings;
    NotificationManager::JobSettings *const m_jobSettings;
    NotificationManager::BadgeSettings *const m_badgeSettings;
    QHash<int, NotificationManager::BehaviorSettings *> m_behaviorSettingsList;
};
