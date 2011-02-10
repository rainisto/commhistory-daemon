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

#include "contactauthorizationlistener.h"
#include "connectionutils.h"
#include "locstrings.h"
#include "constants.h"

// Tp
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/AccountSet>

// Meego
#include <MNotification>

using namespace RTComLogger;

ContactAuthorizationListener::ContactAuthorizationListener(ConnectionUtils *connectionUtils,
                                                           QObject *parent) :
    QObject(parent),
    m_pConnectionUtils(connectionUtils)
{
    qDebug() << Q_FUNC_INFO;

    connect(m_pConnectionUtils,
            SIGNAL(connectionReady(Tp::ConnectionPtr)),
            SLOT(slotConnectionReady(Tp::ConnectionPtr)));

    /*
     * You must be wondering why we're connecting to showAuthorizationDialog from here too. As you
     * can see, we don't have a ContactAuthorizer for a given account before the connection becomes
     * ready. And of course the ContactAuthorizer is the one that takes care of reacting when the
     * notification system tells us that the user wants to authorize/block/ignore a certain
     * contact. Problem is, we cannot perform that operation if the account is not up. So if that's
     * the case, we need to display an informaton banner that says "Sorry, you cannot do this now.
     * Go online first!"
     * We can't do that from the ContactAuthorizer because it doesn't exist yet, as the account is
     * not ready! So we need to do it here. The slot slotShowUnableToAuthorizeDialog will check if
     * the account is online, and if is, then just do nothing. If the account is offline, it will
     * display the banner.
     */
    connect(parent, SIGNAL(showAuthorizationDialog(QString, QString, QString,
                                                   QString, QString, QString)),
            this, SLOT(slotShowUnableToAuthorizeDialog(QString, QString, QString,
                                                       QString, QString, QString)));
}

ContactAuthorizationListener::~ContactAuthorizationListener()
{
    qDebug() << Q_FUNC_INFO;
}

void ContactAuthorizationListener::slotConnectionReady(const Tp::ConnectionPtr& connection)
{
    qDebug() << Q_FUNC_INFO;

    if(!connection)
        return;

    Tp::AccountPtr account;
    Tp::AccountManagerPtr accountManager = m_pConnectionUtils->accountManager();

    if(accountManager && accountManager->isReady()) {
        qDebug() << Q_FUNC_INFO << "Connection object path: " << connection->objectPath();
#if 1
        foreach (Tp::AccountPtr a, accountManager->validAccounts()->accounts()) {
            if (a->connection() && a->connection()->objectPath() == connection->objectPath()) {
                account = a;
                break;
            }
        }
#else   // TODO: enable this back when tp-qt4 declares metatype for Tp::ConnectionPtr
        // and type Tp::Account::connection property
        QVariantMap filter;
        filter.insert(QLatin1String("connection"), QVariant::fromValue(connection));
        Tp::AccountSetPtr accountSet =
                accountManager->filterAccounts(filter);
        if(!accountSet->accounts().isEmpty()){
            account = accountSet->accounts().first();
        }
#endif
    }

    if(account) {
        // Create ContactAuthorizer only for IM accounts:
        if (connection->actualFeatures().contains(Tp::Connection::FeatureSimplePresence))
        {
            qDebug() << Q_FUNC_INFO << "Creating ContactAuthorizer for " << account->uniqueIdentifier();
            /* After connection is ready we need to listen account's connection status changes so that we can
               instantiate ContactAuthorizer again when account goes from offline (ContactAuthorizer destroyed then)
               state to online state: */
            connect(account.data(),
                SIGNAL(connectionStatusChanged(Tp::ConnectionStatus)),
                this,
                SLOT(slotAccountConnectionStatusChanged(Tp::ConnectionStatus)),
                Qt::UniqueConnection);

            ContactAuthorizer *authorizer = new ContactAuthorizer(connection, account, this);
            connect(authorizer, SIGNAL(destroyed(QObject*)), this, SLOT(slotRemoveAuthorizer(QObject*)));

            m_authorizers.insert(account->uniqueIdentifier(), authorizer);
            if(parent()) {
                connect(parent(), SIGNAL(showAuthorizationDialog(QString, QString, QString,
                                                                 QString, QString, QString)),
                        authorizer, SLOT(slotShowAuthorizationDialog(QString, QString, QString,
                                                                     QString, QString, QString)));
            }
        }
    }
}

void ContactAuthorizationListener::slotRemoveAuthorizer(QObject *removedAuthorizer)
{
    qDebug() << Q_FUNC_INFO;

    if (removedAuthorizer) {
        QMutableMapIterator<QString,ContactAuthorizer*> i(m_authorizers);
        while (i.hasNext()) {
            i.next();
            if (i.value() == removedAuthorizer) {
                i.remove();
            }
        }
    }
}

void ContactAuthorizationListener::slotAccountConnectionStatusChanged(Tp::ConnectionStatus connectionStatus)
{
    qDebug() << Q_FUNC_INFO;

    Tp::Account *account = qobject_cast<Tp::Account *>(sender());

    if(account != 0 && account->isValid()) {
        if (connectionStatus == Tp::ConnectionStatusConnected) {
            if (!m_authorizers.contains(account->uniqueIdentifier())) {
                qDebug() << Q_FUNC_INFO << "Connection status changed to Connected";
                Tp::ConnectionPtr connection = account->connection();
                if (!connection.isNull() && connection->isValid()) {
                    if (connection->isReady()) {
                        qDebug() << Q_FUNC_INFO << "Connection is ready";
                        slotConnectionReady(connection);
                    } else {
                        qDebug() << Q_FUNC_INFO << "Asking connection to become ready";
                        connect(connection->becomeReady(Tp::Connection::FeatureSimplePresence),
                                SIGNAL(finished(Tp::PendingOperation*)),
                                m_pConnectionUtils,
                                SLOT(slotConnectionReady(Tp::PendingOperation*)));
                    }
                }
            } else {
                qDebug() << Q_FUNC_INFO << "ContactAuthorizer for account " << account->uniqueIdentifier() << " already exists.";
            }
        } else if (connectionStatus == Tp::ConnectionStatusDisconnected) {
            qDebug() << Q_FUNC_INFO << "Connection status changed to disconnected";
        } else if (connectionStatus == Tp::ConnectionStatusConnecting) {
            qDebug() << Q_FUNC_INFO << "Connection status changed to connecting...";
        } else {
            qDebug() << Q_FUNC_INFO << "Unknown connection status!";
        }
    }
}

void ContactAuthorizationListener::slotShowUnableToAuthorizeDialog(const QString& unused1,
                                                                   const QString& unused2,
                                                                   const QString& unused3,
                                                                   const QString& unused4,
                                                                   const QString& unused5,
                                                                   const QString& accountUniqueIdentifier)
{
    Q_UNUSED(unused1)
    Q_UNUSED(unused2)
    Q_UNUSED(unused3)
    Q_UNUSED(unused4)
    Q_UNUSED(unused5)

    qDebug() << Q_FUNC_INFO;

    if (m_pConnectionUtils == NULL) {
        qWarning() << "ConnectionUtils is NULL.";
        return;
    }

    bool showNotification = false;
    Tp::AccountManagerPtr accountManager = m_pConnectionUtils->accountManager();
    if (accountManager && accountManager->isReady()) {
        QVariantMap filter;
        filter.insert(QLatin1String("uniqueIdentifier"), accountUniqueIdentifier);
        Tp::AccountSetPtr accountSet = accountManager->filterAccounts(filter);
        qDebug() << "Finding out if account is online: " << accountUniqueIdentifier;
        if (!accountSet->accounts().isEmpty()) {
            Tp::AccountPtr account = accountSet->accounts().first();

            if (account.isNull()) {
                qWarning() << "Account is null or invalid.";
                return;
            }

            if (account->connectionStatus() == Tp::ConnectionStatusConnected) {
                qDebug() << "Nothing to do here: the account is online.";
            } else {
                qDebug() << "The account is offline: cannot authorize anyway.";
                showNotification = true;
            }
        }
    } else {
        qDebug() << "Nothing to do here: the account manager is not even ready!";
        showNotification = true;
    }

    if (showNotification) {
        MNotification *notification = new MNotification(MNotification::DeviceEvent);
        notification->setBody(txt_qtn_pers_offline);
        notification->publish();
    }
}

