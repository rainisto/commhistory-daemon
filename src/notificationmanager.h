/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

// QT includes
#include <QObject>
#include <QHash>
#include <QPair>
#include <QDBusInterface>
#include <QFile>
#include <QQueue>
#include <QTimer>
#include <QMultiHash>
#include <QModelIndex>
#include <QWeakPointer>

#include <CommHistory/Event>
#include <CommHistory/Group>
#include <qcontact.h>
QTM_USE_NAMESPACE

// our includes
#include "notificationgroup.h"
#include "personalnotification.h"
#include "reportnotification.h"

class ContextProperty;
class MNotificationGroup;
class MNGFClient;

namespace CommHistory {
    class GroupModel;
}

QTM_BEGIN_NAMESPACE
class QContactManager;
class QContactFetchRequest;
class QContactFilter;
QTM_END_NAMESPACE

namespace RTComLogger {

class MWIListener;
class ChannelListener;

typedef QPair<QString,QString> TpContactUid;

/*!
 * \class NotificationManager
 * \brief class responsible for showing notifications on desktop
 */
class NotificationManager : public QObject
{
    Q_OBJECT

public:
    /*!
     *  \param QObject parent object
     *  \returns Notification manager singleton
     */
    static NotificationManager* instance();

    /*!
     * \brief shows notification
     * \param event to be shown
     */
    void showNotification(ChannelListener *channelListener,
                          const CommHistory::Event& event,
                          const QString &channelTargetId = QString(),
                          CommHistory::Group::ChatType chatType = CommHistory::Group::ChatTypeP2P);

    /*!
     * \brief removes notification
     * \param event type of the group to be removed
     * \returns whether NotificationGroup with event type existed
     */
    bool removeNotificationGroup(int type);

    /*!
     * \brief return group model with all conversations
     * \returns group model pointer
     */
    CommHistory::GroupModel* groupModel();

   /*!
     * \brief Show voicemail notification or removes it if count is 0
     * \param count number of voicemails if it's known,
     *              a negative number if the number is unknown
     */
    void showVoicemailNotification(int count);

private Q_SLOTS:
    /*!
     * Initialises notification manager instance
     */
    void init();
    void slotObservedConversationChanged();
    void slotObservedInboxChanged();
    void slotObservedCallHistoryChanged();
    void slotResultsAvailable();
    void slotResultsAvailableForUnknown();
    void fireNotifications();
    void slotContactsAdded(const QList<QContactLocalId> &contactIds);
    void slotContactsRemoved(const QList<QContactLocalId> &contactIds);
    void slotContactsChanged(const QList<QContactLocalId> &contactIds);
    void fireUnknownContactsRequest();
    void slotOnModelReady();
    void slotGroupRemoved(const QModelIndex &index, int start, int end);
    void slotMWICountChanged(int count);
    void slotChannelClosed(ChannelListener *channelListener);

private:

    NotificationManager( QObject* parent = 0);
    ~NotificationManager();
    bool isCurrentlyObservedByUI(const CommHistory::Event& event,
                                 const QString &channelTargetId,
                                 CommHistory::Group::ChatType chatType);
    void addNotification(PersonalNotification notification);
    NotificationGroup notificationGroup(int type);

    void showLatestNotification(const NotificationGroup& group,
                                PersonalNotification& notification);
    int countContacts(const NotificationGroup& group);
    int countNotifications(const NotificationGroup& group);

    QString action(const NotificationGroup& group,
                   const PersonalNotification& notification,
                   bool grouped);
    QString notificationText(const CommHistory::Event& event);
    QString notificationGroupText(const NotificationGroup& group,
                                  const PersonalNotification& notification);
    QString eventType(int type) const;
    void updateNotificationGroup(const NotificationGroup& group);

    /* actions */
    QString createActionInbox();
    QString createActionCallHistory();
    QString createActionConversation(const QString& accountPath,
                                     const QString& remoteUid,
                                     CommHistory::Group::ChatType chatType);
    QString createActionVoicemail();
    QString activateNotificationRemoteAction(int type,
                                             const QString& action);

    /* persistent notification support */
    void createDataDir();
    bool openStorageFile(QIODevice::OpenModeFlag flag);
    void saveState();
    void loadState();

    /* contacts fetching */
    QContactManager* contactManager();
    void requestContact(TpContactUid contactUid, ChannelListener * = 0);
    void resolveEvents();
    QString contactName(const QString &localUid, const QString &remoteUid);

    /* uses MeeGoTouch notification framework */
    void addGroup(int type);
    void updateGroup(int eventType,
                     const QString& contactName,
                     const QString& message,
                     const QString& action);
    void removeGroup(int type);

    void startNotificationTimer();
    void startContactsTimer();
    bool canShowNotification();

    void removeConversationNotifications(const QString &localId,
                                         const QString &remoteId,
                                         CommHistory::Group::ChatType chatType);

    QContactFetchRequest* startContactRequest(QContactFilter &filter,
                                              const char *resultSlot);
    void updateNotifcationContacts(const QList<QContactLocalId> &contactIds);
    bool hasMessageNotification() const;

private:
    static NotificationManager* m_pInstance;
    QMultiHash<NotificationGroup,PersonalNotification> m_Notifications;
    QHash<int, MNotificationGroup*> m_MgtGroups;
    ContextProperty* m_ObservedConversation;
    ContextProperty* m_ObservedInbox;
    ContextProperty* m_ObservedCallHistory;
    QString m_ObservedChannelLocalId;
    QString m_ObservedChannelRemoteId;
    CommHistory::Group::ChatType m_ObservedChannelChatType;
    QFile m_Storage;
    bool m_Initialised;

    QContactManager *m_pContactManager;
    QQueue<PersonalNotification> m_unresolvedEvents;
    QHash<TpContactUid, QContact> m_contacts;
    QHash<QContactFetchRequest*, TpContactUid> m_requests;
    QHash<QContactFetchRequest*, QWeakPointer<ChannelListener> > m_pendingChannelListeners;
    QHash<QContact, QList<QWeakPointer<ChannelListener> >*> m_channelsPerContact;

    // Delayed notifications
    QTimer m_NotificationTimer;

    CommHistory::GroupModel *m_GroupModel;
    // contact request for unknown/modified group contact
    QContactFilter m_ContactFilter;
    QTimer m_ContactsTimer;

    MWIListener *m_pMWIListener;

    MNGFClient *m_pNgf;

#ifdef UNIT_TEST
    friend class Ut_NotificationManager;
#endif
};

} // namespace RTComLogger

#endif // NOTIFICATIONMANAGER_H
