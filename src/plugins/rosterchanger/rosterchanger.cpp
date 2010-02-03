#include "rosterchanger.h"

#include <QMap>
#include <QDropEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <QDragMoveEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>

#define NOTIFICATOR_ID      "RosterChanger"

#define SVN_AUTOSUBSCRIBE   "autoSubscribe"
#define SVN_AUTOUNSUBSCRIBE "autoUnsubscribe"

#define ADR_STREAM_JID      Action::DR_StreamJid
#define ADR_CONTACT_JID     Action::DR_Parametr1
#define ADR_FROM_STREAM_JID Action::DR_Parametr2
#define ADR_SUBSCRIPTION    Action::DR_Parametr2
#define ADR_NICK            Action::DR_Parametr2
#define ADR_GROUP           Action::DR_Parametr3
#define ADR_REQUEST         Action::DR_Parametr4
#define ADR_TO_GROUP        Action::DR_Parametr4

RosterChanger::RosterChanger()
{
  FPluginManager = NULL;
  FRosterPlugin = NULL;
  FRostersModel = NULL;
  FRostersModel = NULL;
  FRostersView = NULL;
  FNotifications = NULL;
  FSettingsPlugin = NULL;
  FXmppUriQueries = NULL;
  FMultiUserChatPlugin = NULL;

  FOptions = 0;
}

RosterChanger::~RosterChanger()
{

}

//IPlugin
void RosterChanger::pluginInfo(IPluginInfo *APluginInfo)
{
  APluginInfo->name = tr("Roster Editor"); 
  APluginInfo->description = tr("Allows to edit roster");
  APluginInfo->version = "1.0";
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->dependences.append(ROSTER_UUID); 
}

bool RosterChanger::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  FPluginManager = APluginManager;

  IPlugin *plugin = APluginManager->pluginInterface("IRosterPlugin").value(0,NULL);
  if (plugin)
  {
    FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());
    if (FRosterPlugin)
    {
      connect(FRosterPlugin->instance(),SIGNAL(rosterSubscription(IRoster *, const Jid &, int, const QString &)),
        SLOT(onReceiveSubscription(IRoster *, const Jid &, int, const QString &)));
      connect(FRosterPlugin->instance(),SIGNAL(rosterClosed(IRoster *)),SLOT(onRosterClosed(IRoster *)));
    }
  }

  plugin = APluginManager->pluginInterface("IRostersModel").value(0,NULL);
  if (plugin)
    FRostersModel = qobject_cast<IRostersModel *>(plugin->instance());

  plugin = APluginManager->pluginInterface("IRostersViewPlugin").value(0,NULL);
  if (plugin)
  {
    IRostersViewPlugin *rostersViewPlugin = qobject_cast<IRostersViewPlugin *>(plugin->instance());
    if (rostersViewPlugin)
    {
      FRostersView = rostersViewPlugin->rostersView();
      connect(FRostersView->instance(),SIGNAL(indexContextMenu(IRosterIndex *, Menu *)), SLOT(onRosterIndexContextMenu(IRosterIndex *, Menu *)));
    }
  }

  plugin = APluginManager->pluginInterface("INotifications").value(0,NULL);
  if (plugin)
  {
    FNotifications = qobject_cast<INotifications *>(plugin->instance());
    if (FNotifications)
    {
      connect(FNotifications->instance(),SIGNAL(notificationActivated(int)), SLOT(onNotificationActivated(int)));
      connect(FNotifications->instance(),SIGNAL(notificationRemoved(int)), SLOT(onNotificationRemoved(int)));
    }
  }

  plugin = APluginManager->pluginInterface("IMultiUserChatPlugin").value(0,NULL);
  if (plugin)
  {
    FMultiUserChatPlugin = qobject_cast<IMultiUserChatPlugin *>(plugin->instance());
    if (FMultiUserChatPlugin)
    {
      connect(FMultiUserChatPlugin->instance(),SIGNAL(multiUserContextMenu(IMultiUserChatWindow *,IMultiUser *, Menu *)),
        SLOT(onMultiUserContextMenu(IMultiUserChatWindow *,IMultiUser *, Menu *)));
    }
  }

  plugin = APluginManager->pluginInterface("ISettingsPlugin").value(0,NULL);
  if (plugin)
  {
    FSettingsPlugin = qobject_cast<ISettingsPlugin *>(plugin->instance());
    if (FSettingsPlugin)
    {
      connect(FSettingsPlugin->instance(),SIGNAL(settingsOpened()),SLOT(onSettingsOpened()));
      connect(FSettingsPlugin->instance(),SIGNAL(settingsClosed()),SLOT(onSettingsClosed()));
    }
  }

  plugin = APluginManager->pluginInterface("IXmppUriQueries").value(0,NULL);
  if (plugin)
    FXmppUriQueries = qobject_cast<IXmppUriQueries *>(plugin->instance());

  return FRosterPlugin!=NULL;
}

bool RosterChanger::initObjects()
{
  if (FNotifications)
  {
    uchar kindMask = INotification::RosterIcon|INotification::TrayIcon|INotification::TrayAction|INotification::PopupWindow|INotification::PlaySound|INotification::AutoActivate;
    uchar kindDefs = INotification::RosterIcon|INotification::TrayIcon|INotification::TrayAction|INotification::PopupWindow|INotification::PlaySound;
    FNotifications->insertNotificator(NOTIFICATOR_ID,tr("Subscription requests"),kindMask,kindDefs);
  }
  if (FSettingsPlugin)
  {
    FSettingsPlugin->insertOptionsHolder(this);
  }
  if (FRostersView)
  {
    FRostersView->insertDragDropHandler(this);
  }
  if (FXmppUriQueries)
  {
    FXmppUriQueries->insertUriHandler(this, XUHO_DEFAULT);
  }
  return true;
}

//IoptionsHolder
QWidget *RosterChanger::optionsWidget(const QString &ANode, int &AOrder)
{
  if (ANode == ON_ROSTER)
  {
    AOrder = OWO_ROSTER_CHENGER;
    SubscriptionOptions *widget = new SubscriptionOptions(this);
    connect(widget,SIGNAL(optionsAccepted()),SIGNAL(optionsAccepted()));
    connect(FSettingsPlugin->instance(),SIGNAL(optionsDialogAccepted()),widget,SLOT(apply()));
    connect(FSettingsPlugin->instance(),SIGNAL(optionsDialogRejected()),SIGNAL(optionsRejected()));
    return widget;
  }
  return NULL;
}

//IRostersDragDropHandler
Qt::DropActions RosterChanger::rosterDragStart(const QMouseEvent * /*AEvent*/, const QModelIndex &AIndex, QDrag * /*ADrag*/)
{
  int indexType = AIndex.data(RDR_TYPE).toInt();
  if (indexType==RIT_CONTACT || indexType==RIT_GROUP)
    return Qt::CopyAction|Qt::MoveAction;
  return Qt::IgnoreAction;
}

bool RosterChanger::rosterDragEnter(const QDragEnterEvent *AEvent)
{
  if (AEvent->mimeData()->hasFormat(DDT_ROSTERSVIEW_INDEX_DATA))
  {
    QMap<int, QVariant> indexData;
    QDataStream stream(AEvent->mimeData()->data(DDT_ROSTERSVIEW_INDEX_DATA));
    operator>>(stream,indexData);
  
    int indexType = indexData.value(RDR_TYPE).toInt();
    if (indexType==RIT_CONTACT || (indexType==RIT_GROUP && AEvent->source()==FRostersView->instance()))
      return true;
  }
  return false;
}

bool RosterChanger::rosterDragMove(const QDragMoveEvent * /*AEvent*/, const QModelIndex &AHover)
{
  int indexType = AHover.data(RDR_TYPE).toInt();
  if (indexType==RIT_GROUP || indexType==RIT_STREAM_ROOT)
  {
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AHover.data(RDR_STREAM_JID).toString()) : NULL;
    if (roster && roster->isOpen())
      return true;
  }
  return false;
}

void RosterChanger::rosterDragLeave(const QDragLeaveEvent * /*AEvent*/)
{

}

bool RosterChanger::rosterDropAction(const QDropEvent *AEvent, const QModelIndex &AIndex, Menu *AMenu)
{
  int hoverType = AIndex.data(RDR_TYPE).toInt();
  if (AEvent->dropAction()!=Qt::IgnoreAction && (hoverType==RIT_GROUP || hoverType==RIT_STREAM_ROOT))
  {
    Jid hoverStreamJid = AIndex.data(RDR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(hoverStreamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QMap<int, QVariant> indexData;
      QDataStream stream(AEvent->mimeData()->data(DDT_ROSTERSVIEW_INDEX_DATA));
      operator>>(stream,indexData);

      int indexType = indexData.value(RDR_TYPE).toInt();
      Jid indexStreamJid = indexData.value(RDR_STREAM_JID).toString();
      bool isNewContact = indexType==RIT_CONTACT && !roster->rosterItem(indexData.value(RDR_BARE_JID).toString()).isValid;

      if (!isNewContact && (hoverStreamJid && indexStreamJid))
      {
        Action *copyAction = new Action(AMenu);
        copyAction->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_COPY_GROUP);
        copyAction->setData(ADR_STREAM_JID,hoverStreamJid.full());
        copyAction->setData(ADR_TO_GROUP,hoverType==RIT_GROUP ? AIndex.data(RDR_GROUP) : QVariant(QString("")));
        
        Action *moveAction = new Action(AMenu);
        moveAction->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_MOVE_GROUP);
        moveAction->setData(ADR_STREAM_JID,hoverStreamJid.full());
        moveAction->setData(ADR_TO_GROUP,hoverType==RIT_GROUP ? AIndex.data(RDR_GROUP) : QVariant(QString("")));

        if (indexType == RIT_CONTACT)
        {
          copyAction->setText(tr("Copy contact"));
          copyAction->setData(ADR_CONTACT_JID,indexData.value(RDR_BARE_JID));
          connect(copyAction,SIGNAL(triggered(bool)),SLOT(onCopyItemToGroup(bool)));
          AMenu->addAction(copyAction,AG_DEFAULT,true);

          moveAction->setText(tr("Move contact"));
          moveAction->setData(ADR_CONTACT_JID,indexData.value(RDR_BARE_JID));
          moveAction->setData(ADR_GROUP,indexData.value(RDR_GROUP));
          connect(moveAction,SIGNAL(triggered(bool)),SLOT(onMoveItemToGroup(bool)));
          AMenu->addAction(moveAction,AG_DEFAULT,true);
        }
        else
        {
          copyAction->setText(tr("Copy group"));
          copyAction->setData(ADR_GROUP,indexData.value(RDR_GROUP));
          connect(copyAction,SIGNAL(triggered(bool)),SLOT(onCopyGroupToGroup(bool)));
          AMenu->addAction(copyAction,AG_DEFAULT,true);

          moveAction->setText(tr("Move group"));
          moveAction->setData(ADR_GROUP,indexData.value(RDR_GROUP));
          connect(moveAction,SIGNAL(triggered(bool)),SLOT(onMoveGroupToGroup(bool)));
          AMenu->addAction(moveAction,AG_DEFAULT,true);
        }
        AMenu->setDefaultAction(AEvent->dropAction()==Qt::MoveAction ? moveAction : copyAction);
        return true;
      }
      else
      {
        Action *copyAction = new Action(AMenu);
        copyAction->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_ADD_CONTACT);
        copyAction->setData(ADR_STREAM_JID,hoverStreamJid.full());
        copyAction->setData(ADR_TO_GROUP,hoverType==RIT_GROUP ? AIndex.data(RDR_GROUP) : QVariant(QString("")));

        if (indexType == RIT_CONTACT)
        {
          copyAction->setText(isNewContact ? tr("Add contact") : tr("Copy contact"));
          copyAction->setData(ADR_CONTACT_JID,indexData.value(RDR_BARE_JID));
          copyAction->setData(ADR_NICK,indexData.value(RDR_NAME));
          connect(copyAction,SIGNAL(triggered(bool)),SLOT(onAddItemToGroup(bool)));
          AMenu->addAction(copyAction,AG_DEFAULT,true);
        }
        else
        {
          copyAction->setText(tr("Copy group"));
          copyAction->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_COPY_GROUP);
          copyAction->setData(ADR_FROM_STREAM_JID,indexStreamJid.full());
          copyAction->setData(ADR_GROUP,indexData.value(RDR_GROUP));
          connect(copyAction,SIGNAL(triggered(bool)),SLOT(onAddGroupToGroup(bool)));
          AMenu->addAction(copyAction,AG_DEFAULT,true);
        }
        AMenu->setDefaultAction(copyAction);
        return true;
      }
    }
  }
  return false;
}

bool RosterChanger::xmppUriOpen(const Jid &AStreamJid, const Jid &AContactJid, const QString &AAction, const QMultiMap<QString, QString> &AParams)
{
  if (AAction == "roster")
  {
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
    if (roster && roster->isOpen() && !roster->rosterItem(AContactJid).isValid)
    {
      IAddContactDialog *dialog = showAddContactDialog(AStreamJid);
      if (dialog)
      {
        dialog->setContactJid(AContactJid);
        dialog->setNickName(AParams.contains("name") ? AParams.value("name") : AContactJid.node());
        dialog->setGroup(AParams.contains("group") ? AParams.value("group") : QString::null);
        dialog->instance()->show();
      }
    }
    return true;
  }
  else if (AAction == "remove")
  {
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
    if (roster && roster->isOpen() && roster->rosterItem(AContactJid).isValid)
    {
      if (QMessageBox::question(NULL, tr("Remove contact"), 
        tr("You are assured that wish to remove a contact <b>%1</b> from roster?").arg(AContactJid.hBare()),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
      {
        roster->removeItem(AContactJid);
      }
    }
    return true;
  }
  else if (AAction == "subscribe")
  {
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
    const IRosterItem &ritem = roster!=NULL ? roster->rosterItem(AContactJid) : IRosterItem();
    if (roster && roster->isOpen() && ritem.subscription!=SUBSCRIPTION_BOTH && ritem.subscription!=SUBSCRIPTION_TO)
    {
      if (QMessageBox::question(NULL, tr("Subscribe for contact presence"), 
        tr("You are assured that wish to subscribe for a contact <b>%1</b> presence?").arg(AContactJid.hBare()),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
      {
        roster->sendSubscription(AContactJid, IRoster::Subscribe);
      }
    }
    return true;
  }
  else if (AAction == "unsubscribe")
  {
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
    const IRosterItem &ritem = roster!=NULL ? roster->rosterItem(AContactJid) : IRosterItem();
    if (roster && roster->isOpen() && ritem.subscription!=SUBSCRIPTION_NONE && ritem.subscription!=SUBSCRIPTION_FROM)
    {
      if (QMessageBox::question(NULL, tr("Unsubscribe from contact presence"), 
        tr("You are assured that wish to unsubscribe from a contact <b>%1</b> presence?").arg(AContactJid.hBare()),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
      {
        roster->sendSubscription(AContactJid, IRoster::Unsubscribe);
      }
    }
    return true;
  }
  return false;
}

//IRosterChanger
bool RosterChanger::isAutoSubscribe(const Jid &AStreamJid, const Jid &AContactJid) const
{
  if (FAutoSubscriptions.value(AStreamJid).contains(AContactJid.bare()))
    return (FAutoSubscriptions.value(AStreamJid).value(AContactJid.bare()).autoOptions & IRosterChanger::AutoSubscribe) > 0;
  return checkOption(AutoSubscribe);
}

bool RosterChanger::isAutoUnsubscribe(const Jid &AStreamJid, const Jid &AContactJid) const
{
  if (FAutoSubscriptions.value(AStreamJid).contains(AContactJid.bare()))
    return (FAutoSubscriptions.value(AStreamJid).value(AContactJid.bare()).autoOptions & IRosterChanger::AutoUnsubscribe) > 0;
  return checkOption(AutoUnsubscribe);
}

bool RosterChanger::isSilentSubsctiption(const Jid &AStreamJid, const Jid &AContactJid) const
{
  if (FAutoSubscriptions.value(AStreamJid).contains(AContactJid.bare()))
    return FAutoSubscriptions.value(AStreamJid).value(AContactJid.bare()).silent;
  return false;
}

void RosterChanger::insertAutoSubscribe(const Jid &AStreamJid, const Jid &AContactJid, int AAutoOptions, bool ASilently)
{
  AutoSubscription &asubscr = FAutoSubscriptions[AStreamJid][AContactJid.bare()];
  asubscr.silent = ASilently;
  asubscr.autoOptions = AAutoOptions;
}

void RosterChanger::removeAutoSubscribe(const Jid &AStreamJid, const Jid &AContactJid)
{
  FAutoSubscriptions[AStreamJid].remove(AContactJid.bare());
}

void RosterChanger::subscribeContact(const Jid &AStreamJid, const Jid &AContactJid, const QString &AMessage, bool ASilently)
{
  IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
  if (roster && roster->isOpen())
  {
    const IRosterItem &ritem = roster->rosterItem(AContactJid);
    roster->sendSubscription(AContactJid,IRoster::Subscribed,AMessage);
    if (ritem.subscription!=SUBSCRIPTION_TO && ritem.subscription!=SUBSCRIPTION_BOTH)
    {
      insertAutoSubscribe(AStreamJid,AContactJid,AutoSubscribe,ASilently);
      roster->sendSubscription(AContactJid,IRoster::Subscribe,AMessage);
    }
  }
}

void RosterChanger::unsubscribeContact(const Jid &AStreamJid, const Jid &AContactJid, const QString &AMessage, bool ASilently)
{
  IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
  if (roster && roster->isOpen())
  {
    const IRosterItem &ritem = roster->rosterItem(AContactJid);
    roster->sendSubscription(AContactJid,IRoster::Unsubscribed,AMessage);
    if (ritem.subscription!=SUBSCRIPTION_FROM && ritem.subscription!=SUBSCRIPTION_NONE)
    {
      insertAutoSubscribe(AStreamJid,AContactJid,AutoUnsubscribe,ASilently);
      roster->sendSubscription(AContactJid,IRoster::Unsubscribe,AMessage);
    }
  }
}

bool RosterChanger::checkOption(IRosterChanger::Option AOption) const
{
  return (FOptions & AOption) > 0;
}

void RosterChanger::setOption(IRosterChanger::Option AOption, bool AValue)
{
  bool changed = checkOption(AOption) != AValue;
  if (changed)
  {
    AValue ? FOptions |= AOption : FOptions &= ~AOption;
    emit optionChanged(AOption,AValue);
  }
}

IAddContactDialog *RosterChanger::showAddContactDialog(const Jid &AStreamJid)
{
  IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
  if (roster && roster->isOpen())
  {
    AddContactDialog *dialog = new AddContactDialog(this,FPluginManager,AStreamJid);
    connect(roster->instance(),SIGNAL(closed()),dialog,SLOT(reject()));
    emit addContactDialogCreated(dialog);
    dialog->show();
    return dialog;
  }
  return NULL;
}

QString RosterChanger::subscriptionNotify(int ASubsType, const Jid &AContactJid) const
{
  switch(ASubsType) 
  {
  case IRoster::Subscribe:
    return tr("%1 wants to subscribe to your presence.").arg(AContactJid.hBare());
  case IRoster::Subscribed:
    return tr("You are now subscribed for %1 presence.").arg(AContactJid.hBare());
  case IRoster::Unsubscribe:
    return tr("%1 unsubscribed from your presence.").arg(AContactJid.hBare());
  case IRoster::Unsubscribed:
    return tr("You are now unsubscribed from %1 presence.").arg(AContactJid.hBare());
  }
  return QString::null;
}

Menu *RosterChanger::createGroupMenu(const QHash<int,QVariant> &AData, const QSet<QString> &AExceptGroups, 
                                     bool ANewGroup, bool ARootGroup, const char *ASlot, Menu *AParent)
{
  Menu *menu = new Menu(AParent);
  IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AData.value(ADR_STREAM_JID).toString()) : NULL;
  if (roster)
  {
    QString group;
    QString groupDelim = roster->groupDelimiter();
    QHash<QString,Menu *> menus;
    QSet<QString> allGroups = roster->groups();
    foreach(group,allGroups)
    {
      Menu *parentMenu = menu;
      QList<QString> groupTree = group.split(groupDelim,QString::SkipEmptyParts);
      QString groupName;
      int index = 0;
      while (index < groupTree.count())
      {
        if (groupName.isEmpty())
          groupName = groupTree.at(index);
        else
          groupName += groupDelim + groupTree.at(index);

        if (!menus.contains(groupName))
        {
          Menu *groupMenu = new Menu(parentMenu);
          groupMenu->setTitle(groupTree.at(index));
          groupMenu->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_GROUP);

          if (!AExceptGroups.contains(groupName))
          {
            Action *curGroupAction = new Action(groupMenu);
            curGroupAction->setText(tr("This group"));
            curGroupAction->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_THIS_GROUP);
            curGroupAction->setData(AData);
            curGroupAction->setData(ADR_TO_GROUP,groupName);
            connect(curGroupAction,SIGNAL(triggered(bool)),ASlot);
            groupMenu->addAction(curGroupAction,AG_RVCM_ROSTERCHANGER+1);
          }

          if (ANewGroup)
          {
            Action *newGroupAction = new Action(groupMenu);
            newGroupAction->setText(tr("Create new..."));
            newGroupAction->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_CREATE_GROUP);
            newGroupAction->setData(AData);
            newGroupAction->setData(ADR_TO_GROUP,groupName+groupDelim);
            connect(newGroupAction,SIGNAL(triggered(bool)),ASlot);
            groupMenu->addAction(newGroupAction,AG_RVCM_ROSTERCHANGER+1);
          }

          menus.insert(groupName,groupMenu);
          parentMenu->addAction(groupMenu->menuAction(),AG_RVCM_ROSTERCHANGER,true);
          parentMenu = groupMenu;
        }
        else
          parentMenu = menus.value(groupName); 

        index++;
      }
    }

    if (ARootGroup)
    {
      Action *curGroupAction = new Action(menu);
      curGroupAction->setText(tr("Root"));
      curGroupAction->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_ROOT_GROUP);
      curGroupAction->setData(AData);
      curGroupAction->setData(ADR_TO_GROUP,"");
      connect(curGroupAction,SIGNAL(triggered(bool)),ASlot);
      menu->addAction(curGroupAction,AG_RVCM_ROSTERCHANGER+1);
    }

    if (ANewGroup)
    {
      Action *newGroupAction = new Action(menu);
      newGroupAction->setText(tr("Create new..."));
      newGroupAction->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_CREATE_GROUP);
      newGroupAction->setData(AData);
      newGroupAction->setData(ADR_TO_GROUP,groupDelim);
      connect(newGroupAction,SIGNAL(triggered(bool)),ASlot);
      menu->addAction(newGroupAction,AG_RVCM_ROSTERCHANGER+1);
    }
  }
  return menu;
}

SubscriptionDialog *RosterChanger::findSubscriptionDialog(const Jid &AStreamJid, const Jid &AContactJid) const
{
  foreach (SubscriptionDialog *dialog, FNotifyDialog)
    if (dialog && dialog->streamJid()==AStreamJid && dialog->contactJid()==AContactJid)
      return dialog;
  return NULL;
}

SubscriptionDialog *RosterChanger::createSubscriptionDialog(const Jid &AStreamJid, const Jid &AContactJid, const QString &ANotify, const QString &AMessage)
{
  SubscriptionDialog *dialog = findSubscriptionDialog(AStreamJid,AContactJid);
  if (dialog == NULL)
  {
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
    if (roster && roster->isOpen())
    {
      dialog = new SubscriptionDialog(this,FPluginManager,AStreamJid,AContactJid,ANotify,AMessage);
      connect(roster->instance(),SIGNAL(closed()),dialog->instance(),SLOT(reject()));
      connect(dialog,SIGNAL(dialogDestroyed()),SLOT(onSubscriptionDialogDestroyed()));
      emit subscriptionDialogCreated(dialog);
    }
  }
  return dialog;
}

void RosterChanger::onSettingsOpened()
{
  ISettings *settings = FSettingsPlugin->settingsForPlugin(ROSTERCHANGER_UUID);
  setOption(IRosterChanger::AutoSubscribe,settings->value(SVN_AUTOSUBSCRIBE,false).toBool());
  setOption(IRosterChanger::AutoUnsubscribe,settings->value(SVN_AUTOUNSUBSCRIBE,true).toBool());
}

void RosterChanger::onSettingsClosed()
{
  ISettings *settings = FSettingsPlugin->settingsForPlugin(ROSTERCHANGER_UUID);
  settings->setValue(SVN_AUTOSUBSCRIBE,checkOption(IRosterChanger::AutoSubscribe));
  settings->setValue(SVN_AUTOUNSUBSCRIBE,checkOption(IRosterChanger::AutoUnsubscribe));
}

void RosterChanger::onShowAddContactDialog(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    IAddContactDialog *dialog = showAddContactDialog(action->data(ADR_STREAM_JID).toString());
    if (dialog)
    {
      dialog->setContactJid(action->data(ADR_CONTACT_JID).toString());
      dialog->setNickName(action->data(ADR_NICK).toString());
      dialog->setGroup(action->data(ADR_GROUP).toString());
      dialog->setSubscriptionMessage(action->data(Action::DR_Parametr4).toString());
    }
  }
}

void RosterChanger::onRosterIndexContextMenu(IRosterIndex *AIndex, Menu *AMenu)
{
  QString streamJid = AIndex->data(RDR_STREAM_JID).toString();
  IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
  if (roster && roster->isOpen())
  {
    int itemType = AIndex->data(RDR_TYPE).toInt();
    IRosterItem ritem = roster->rosterItem(AIndex->data(RDR_BARE_JID).toString());
    if (itemType == RIT_STREAM_ROOT)
    {
      Action *action = new Action(AMenu);
      action->setText(tr("Add contact"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_ADD_CONTACT);
      action->setData(ADR_STREAM_JID,AIndex->data(RDR_JID));
      connect(action,SIGNAL(triggered(bool)),SLOT(onShowAddContactDialog(bool)));
      AMenu->addAction(action,AG_RVCM_ROSTERCHANGER_ADD_CONTACT,true);
    }
    else if (itemType == RIT_CONTACT || itemType == RIT_AGENT)
    {
      QHash<int,QVariant> data;
      data.insert(ADR_STREAM_JID,streamJid);
      data.insert(ADR_CONTACT_JID,AIndex->data(RDR_BARE_JID).toString());
      
      Menu *subsMenu = new Menu(AMenu);
      subsMenu->setTitle(tr("Subscription"));
      subsMenu->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_SUBSCRIBTION);

      Action *action = new Action(subsMenu);
      action->setText(tr("Subscribe contact"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_SUBSCRIBE);
      action->setData(data);
      action->setData(ADR_SUBSCRIPTION,IRoster::Subscribe);
      connect(action,SIGNAL(triggered(bool)),SLOT(onContactSubscription(bool)));
      subsMenu->addAction(action,AG_DEFAULT-1);

      action = new Action(subsMenu);
      action->setText(tr("Unsubscribe contact"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_UNSUBSCRIBE);
      action->setData(data);
      action->setData(ADR_SUBSCRIPTION,IRoster::Unsubscribe);
      connect(action,SIGNAL(triggered(bool)),SLOT(onContactSubscription(bool)));
      subsMenu->addAction(action,AG_DEFAULT-1);

      action = new Action(subsMenu);
      action->setText(tr("Send"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_SUBSCR_SEND);
      action->setData(data);
      action->setData(ADR_SUBSCRIPTION,IRoster::Subscribed);
      connect(action,SIGNAL(triggered(bool)),SLOT(onSendSubscription(bool)));
      subsMenu->addAction(action);

      action = new Action(subsMenu);
      action->setText(tr("Request"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_SUBSCR_REQUEST);
      action->setData(data);
      action->setData(ADR_SUBSCRIPTION,IRoster::Subscribe);
      connect(action,SIGNAL(triggered(bool)),SLOT(onSendSubscription(bool)));
      subsMenu->addAction(action);

      action = new Action(subsMenu);
      action->setText(tr("Remove"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_SUBSCR_REMOVE);
      action->setData(data);
      action->setData(ADR_SUBSCRIPTION,IRoster::Unsubscribed);
      connect(action,SIGNAL(triggered(bool)),SLOT(onSendSubscription(bool)));
      subsMenu->addAction(action);

      action = new Action(subsMenu);
      action->setText(tr("Refuse"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_SUBSCR_REFUSE);
      action->setData(data);
      action->setData(ADR_SUBSCRIPTION,IRoster::Unsubscribe);
      connect(action,SIGNAL(triggered(bool)),SLOT(onSendSubscription(bool)));
      subsMenu->addAction(action);

      AMenu->addAction(subsMenu->menuAction(),AG_RVCM_ROSTERCHANGER_SUBSCRIPTION);

      if (ritem.isValid)
      {
        QSet<QString> exceptGroups = ritem.groups;

        data.insert(ADR_NICK,AIndex->data(RDR_NAME));
        data.insert(ADR_GROUP,AIndex->data(RDR_GROUP));

        action = new Action(AMenu);
        action->setText(tr("Rename..."));
        action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_RENAME);
        action->setData(data);
        connect(action,SIGNAL(triggered(bool)),SLOT(onRenameItem(bool)));
        AMenu->addAction(action,AG_RVCM_ROSTERCHANGER);

        if (itemType == RIT_CONTACT)
        {
          Menu *copyItem = createGroupMenu(data,exceptGroups,true,false,SLOT(onCopyItemToGroup(bool)),AMenu);
          copyItem->setTitle(tr("Copy to group"));
          copyItem->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_COPY_GROUP);
          AMenu->addAction(copyItem->menuAction(),AG_RVCM_ROSTERCHANGER);

          Menu *moveItem = createGroupMenu(data,exceptGroups,true,false,SLOT(onMoveItemToGroup(bool)),AMenu);
          moveItem->setTitle(tr("Move to group"));
          moveItem->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_MOVE_GROUP);
          AMenu->addAction(moveItem->menuAction(),AG_RVCM_ROSTERCHANGER);
        }

        if (!AIndex->data(RDR_GROUP).toString().isEmpty())
        {
          action = new Action(AMenu);
          action->setText(tr("Remove from group"));
          action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_REMOVE_FROM_GROUP);
          action->setData(data);
          connect(action,SIGNAL(triggered(bool)),SLOT(onRemoveItemFromGroup(bool)));
          AMenu->addAction(action,AG_RVCM_ROSTERCHANGER);
        }
      }
      else
      {
        action = new Action(AMenu);
        action->setText(tr("Add contact"));
        action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_ADD_CONTACT);
        action->setData(ADR_STREAM_JID,streamJid);
        action->setData(ADR_CONTACT_JID,AIndex->data(RDR_JID));
        connect(action,SIGNAL(triggered(bool)),SLOT(onShowAddContactDialog(bool)));
        AMenu->addAction(action,AG_RVCM_ROSTERCHANGER_ADD_CONTACT,true);
      }

      action = new Action(AMenu);
      action->setText(tr("Remove from roster"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_REMOVE_CONTACT);
      action->setData(data);
      connect(action,SIGNAL(triggered(bool)),SLOT(onRemoveItemFromRoster(bool)));
      AMenu->addAction(action,AG_RVCM_ROSTERCHANGER);
    }
    else if (itemType == RIT_GROUP)
    {
      QHash<int,QVariant> data;
      data.insert(ADR_STREAM_JID,streamJid);
      data.insert(ADR_GROUP,AIndex->data(RDR_GROUP));
      
      Action *action = new Action(AMenu);
      action->setText(tr("Add contact"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_ADD_CONTACT);
      action->setData(data);
      connect(action,SIGNAL(triggered(bool)),SLOT(onShowAddContactDialog(bool)));
      AMenu->addAction(action,AG_RVCM_ROSTERCHANGER_ADD_CONTACT,true);

      action = new Action(AMenu);
      action->setText(tr("Rename..."));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_RENAME);
      action->setData(data);
      connect(action,SIGNAL(triggered(bool)),SLOT(onRenameGroup(bool)));
      AMenu->addAction(action,AG_RVCM_ROSTERCHANGER);

      QSet<QString> exceptGroups;
      exceptGroups << AIndex->data(RDR_GROUP).toString();

      Menu *copyGroup = createGroupMenu(data,exceptGroups,true,true,SLOT(onCopyGroupToGroup(bool)),AMenu);
      copyGroup->setTitle(tr("Copy to group"));
      copyGroup->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_COPY_GROUP);
      AMenu->addAction(copyGroup->menuAction(),AG_RVCM_ROSTERCHANGER);

      Menu *moveGroup = createGroupMenu(data,exceptGroups,true,true,SLOT(onMoveGroupToGroup(bool)),AMenu);
      moveGroup->setTitle(tr("Move to group"));
      moveGroup->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_MOVE_GROUP);
      AMenu->addAction(moveGroup->menuAction(),AG_RVCM_ROSTERCHANGER);

      action = new Action(AMenu);
      action->setText(tr("Remove group"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_REMOVE_GROUP);
      action->setData(data);
      connect(action,SIGNAL(triggered(bool)),SLOT(onRemoveGroup(bool)));
      AMenu->addAction(action,AG_RVCM_ROSTERCHANGER);

      action = new Action(AMenu);
      action->setText(tr("Remove contacts"));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_REMOVE_CONTACTS);
      action->setData(data);
      connect(action,SIGNAL(triggered(bool)),SLOT(onRemoveGroupItems(bool)));
      AMenu->addAction(action,AG_RVCM_ROSTERCHANGER);
    }
  }
}

void RosterChanger::onContactSubscription(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QString contactJid = action->data(ADR_CONTACT_JID).toString();
      int subsType = action->data(ADR_SUBSCRIPTION).toInt();
      if (subsType == IRoster::Subscribe)
        subscribeContact(streamJid,contactJid);
      else if (subsType == IRoster::Unsubscribe)
        unsubscribeContact(streamJid,contactJid);
    }
  }
}

void RosterChanger::onSendSubscription(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QString rosterJid = action->data(ADR_CONTACT_JID).toString();
      int subsType = action->data(ADR_SUBSCRIPTION).toInt();
      roster->sendSubscription(rosterJid,subsType);
    }
  }
}

void RosterChanger::onReceiveSubscription(IRoster *ARoster, const Jid &AContactJid, int ASubsType, const QString &AMessage)
{
  INotification notify;
  if (FNotifications)
  {
    notify.kinds = INotifications::EnablePopupWindows;
    notify.data.insert(NDR_ICON,IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_RCHANGER_SUBSCRIBTION));
    notify.data.insert(NDR_TOOLTIP,tr("Subscription message from %1").arg(FNotifications->contactName(ARoster->streamJid(),AContactJid)));
    notify.data.insert(NDR_ROSTER_STREAM_JID,ARoster->streamJid().full());
    notify.data.insert(NDR_ROSTER_CONTACT_JID,AContactJid.full());
    notify.data.insert(NDR_ROSTER_NOTIFY_ORDER,RLO_SUBSCRIBTION);
    notify.data.insert(NDR_WINDOW_CAPTION, tr("Subscription message"));
    notify.data.insert(NDR_WINDOW_TITLE,FNotifications->contactName(ARoster->streamJid(),AContactJid));
    notify.data.insert(NDR_WINDOW_IMAGE, FNotifications->contactAvatar(AContactJid));
    notify.data.insert(NDR_WINDOW_TEXT,subscriptionNotify(ASubsType,AContactJid));
    notify.data.insert(NDR_SOUND_FILE,SDF_RCHANGER_SUBSCRIPTION);
  }

  int notifyId = -1;
  SubscriptionDialog *dialog = NULL;
  const IRosterItem &ritem = ARoster->rosterItem(AContactJid);
  if (ASubsType == IRoster::Subscribe)
  {
    bool autoSubscribe = isAutoSubscribe(ARoster->streamJid(),AContactJid);
    if (!autoSubscribe && ritem.subscription!=SUBSCRIPTION_FROM && ritem.subscription!=SUBSCRIPTION_BOTH)
    {
      dialog = createSubscriptionDialog(ARoster->streamJid(),AContactJid,subscriptionNotify(ASubsType,AContactJid),AMessage);
      if (dialog!=NULL && !FNotifyDialog.values().contains(dialog))
      {
        if (FNotifications)
        {
          notify.kinds = FNotifications->notificatorKinds(NOTIFICATOR_ID);
          notifyId = FNotifications->appendNotification(notify);
        }
        else
        {
          dialog->instance()->show();
        }
      }
    }
    else
    {
      ARoster->sendSubscription(AContactJid,IRoster::Subscribed);
      if (autoSubscribe && ritem.subscription!=SUBSCRIPTION_TO && ritem.subscription!=SUBSCRIPTION_BOTH)
        ARoster->sendSubscription(AContactJid,IRoster::Subscribe);
    }
  }
  else if (ASubsType == IRoster::Unsubscribed)
  {
    if (FNotifications && !isSilentSubsctiption(ARoster->streamJid(),AContactJid))
      notifyId = FNotifications->appendNotification(notify);
    
    if (isAutoUnsubscribe(ARoster->streamJid(),AContactJid))
      ARoster->sendSubscription(AContactJid,IRoster::Unsubscribed);
  }
  else  if (ASubsType == IRoster::Subscribed)
  {
    if (FNotifications && !isSilentSubsctiption(ARoster->streamJid(),AContactJid))
      notifyId = FNotifications->appendNotification(notify);
  }
  else if (ASubsType == IRoster::Unsubscribe)
  {
    if (FNotifications && !isSilentSubsctiption(ARoster->streamJid(),AContactJid))
      notifyId = FNotifications->appendNotification(notify);
  }

  if (notifyId>0 || dialog!=NULL)
    FNotifyDialog.insert(notifyId,dialog);
}

void RosterChanger::onAddItemToGroup(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      Jid rosterJid = action->data(ADR_CONTACT_JID).toString();
      QString groupName = action->data(ADR_TO_GROUP).toString();
      IRosterItem ritem = roster->rosterItem(rosterJid);
      if (!ritem.isValid)
      {
        QString nick = action->data(ADR_NICK).toString();
        roster->setItem(rosterJid,nick,QSet<QString>()<<groupName);
      }
      else
      {
        roster->copyItemToGroup(rosterJid,groupName);
      }
    }
  }
}

void RosterChanger::onRenameItem(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      Jid rosterJid = action->data(ADR_CONTACT_JID).toString();
      QString oldName = action->data(ADR_NICK).toString();
      bool ok = false;
      QString newName = QInputDialog::getText(NULL,tr("Rename contact"),tr("Enter name for: <b>%1</b>").arg(rosterJid.hBare()),
        QLineEdit::Normal,oldName,&ok);
      if (ok && !newName.isEmpty() && newName != oldName)
        roster->renameItem(rosterJid,newName);
    }
  }
}

void RosterChanger::onCopyItemToGroup(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QString groupDelim = roster->groupDelimiter();
      QString rosterJid = action->data(ADR_CONTACT_JID).toString();
      QString groupName = action->data(ADR_TO_GROUP).toString();
      if (groupName.endsWith(groupDelim))
      {
        bool ok = false;
        QString newGroupName = QInputDialog::getText(NULL,tr("Create new group"),tr("Enter group name:"),
          QLineEdit::Normal,QString(),&ok);
        if (ok && !newGroupName.isEmpty())
        {
          if (groupName == groupDelim)
            groupName = newGroupName;
          else
            groupName+=newGroupName;
          roster->copyItemToGroup(rosterJid,groupName);
        }
      }
      else
        roster->copyItemToGroup(rosterJid,groupName);
    }
  }
}

void RosterChanger::onMoveItemToGroup(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QString groupDelim = roster->groupDelimiter();
      QString rosterJid = action->data(ADR_CONTACT_JID).toString();
      QString groupName = action->data(ADR_GROUP).toString();
      QString moveToGroup = action->data(ADR_TO_GROUP).toString();
      if (moveToGroup.endsWith(groupDelim))
      {
        bool ok = false;
        QString newGroupName = QInputDialog::getText(NULL,tr("Create new group"),tr("Enter group name:"),
          QLineEdit::Normal,QString(),&ok);
        if (ok && !newGroupName.isEmpty())
        {
          if (moveToGroup == groupDelim)
            moveToGroup = newGroupName;
          else
            moveToGroup+=newGroupName;
          roster->moveItemToGroup(rosterJid,groupName,moveToGroup);
        }
      }
      else
        roster->moveItemToGroup(rosterJid,groupName,moveToGroup);
    }
  }
}

void RosterChanger::onRemoveItemFromGroup(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QString rosterJid = action->data(ADR_CONTACT_JID).toString();
      QString groupName = action->data(ADR_GROUP).toString();
      roster->removeItemFromGroup(rosterJid,groupName);
    }
  }
}

void RosterChanger::onRemoveItemFromRoster(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      Jid rosterJid = action->data(ADR_CONTACT_JID).toString();
      if (roster->rosterItem(rosterJid).isValid)
      {
        if (QMessageBox::question(NULL,tr("Remove contact"),
          tr("You are assured that wish to remove a contact <b>%1</b> from roster?").arg(rosterJid.hBare()),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
          roster->removeItem(rosterJid);
        }
      }
      else if (FRostersModel)
      {
        QMultiHash<int, QVariant> data;
        data.insert(RDR_TYPE,RIT_CONTACT);
        data.insert(RDR_TYPE,RIT_AGENT);
        data.insert(RDR_BARE_JID,rosterJid.pBare());
        IRosterIndex *streamIndex = FRostersModel->streamRoot(streamJid);
        foreach(IRosterIndex *index, streamIndex->findChild(data,true))
          FRostersModel->removeRosterIndex(index);
      }
    }
  }
}

void RosterChanger::onAddGroupToGroup(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString toStreamJid = action->data(ADR_STREAM_JID).toString();
    QString fromStreamJid = action->data(ADR_FROM_STREAM_JID).toString();
    IRoster *toRoster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(toStreamJid) : NULL;
    IRoster *fromRoster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(fromStreamJid) : NULL;
    if (fromRoster && toRoster && toRoster->isOpen())
    {
      QString toGroup = action->data(ADR_TO_GROUP).toString();
      QString fromGroup = action->data(ADR_GROUP).toString();
      QString fromGroupLast = fromGroup.split(fromRoster->groupDelimiter(),QString::SkipEmptyParts).last();
      
      QList<IRosterItem> toItems;
      QList<IRosterItem> fromItems = fromRoster->groupItems(fromGroup);
      foreach(IRosterItem fromItem, fromItems)
      {
        QSet<QString> newGroups;
        foreach(QString group, fromItem.groups)
        {
          if (group.startsWith(fromGroup))
          {
            QString newGroup = group;
            newGroup.remove(0,fromGroup.size());
            if (!toGroup.isEmpty())
              newGroup.prepend(toGroup + toRoster->groupDelimiter() + fromGroupLast);
            else
              newGroup.prepend(fromGroupLast);
            newGroups += newGroup;
          }
        }
        IRosterItem toItem = toRoster->rosterItem(fromItem.itemJid);
        if (!toItem.isValid)
        {
          toItem.isValid = true;
          toItem.itemJid = fromItem.itemJid;
          toItem.name = fromItem.name;
          toItem.groups = newGroups;
        }
        else
        {
          toItem.groups += newGroups;
        }
        toItems.append(toItem);
      }
      toRoster->setItems(toItems);
    }
  }
}

void RosterChanger::onRenameGroup(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      bool ok = false;
      QString groupDelim = roster->groupDelimiter();
      QString groupName = action->data(ADR_GROUP).toString();
      QList<QString> groupTree = groupName.split(groupDelim,QString::SkipEmptyParts);
      
      QString newGroupPart = QInputDialog::getText(NULL,tr("Rename group"),tr("Enter new group name:"),
        QLineEdit::Normal,groupTree.last(),&ok);
      
      if (ok && !newGroupPart.isEmpty())
      {
        QString newGroupName = groupName;
        newGroupName.chop(groupTree.last().size());
        newGroupName += newGroupPart;
        roster->renameGroup(groupName,newGroupName);
      }
    }
  }
}

void RosterChanger::onCopyGroupToGroup(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QString groupDelim = roster->groupDelimiter();
      QString groupName = action->data(ADR_GROUP).toString();
      QString copyToGroup = action->data(ADR_TO_GROUP).toString();
      if (copyToGroup.endsWith(groupDelim))
      {
        bool ok = false;
        QString newGroupName = QInputDialog::getText(NULL,tr("Create new group"),tr("Enter group name:"),
          QLineEdit::Normal,QString(),&ok);
        if (ok && !newGroupName.isEmpty())
        {
          if (copyToGroup == groupDelim)
            copyToGroup = newGroupName;
          else
            copyToGroup+=newGroupName;
          roster->copyGroupToGroup(groupName,copyToGroup);
        }
      }
      else
        roster->copyGroupToGroup(groupName,copyToGroup);
    }
  }
}

void RosterChanger::onMoveGroupToGroup(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QString groupDelim = roster->groupDelimiter();
      QString groupName = action->data(ADR_GROUP).toString();
      QString moveToGroup = action->data(ADR_TO_GROUP).toString();
      if (moveToGroup.endsWith(roster->groupDelimiter()))
      {
        bool ok = false;
        QString newGroupName = QInputDialog::getText(NULL,tr("Create new group"),tr("Enter group name:"),
          QLineEdit::Normal,QString(),&ok);
        if (ok && !newGroupName.isEmpty())
        {
          if (moveToGroup == groupDelim)
            moveToGroup = newGroupName;
          else
            moveToGroup+=newGroupName;
          roster->moveGroupToGroup(groupName,moveToGroup);
        }
      }
      else
        roster->moveGroupToGroup(groupName,moveToGroup);
    }
  }
}

void RosterChanger::onRemoveGroup(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QString groupName = action->data(ADR_GROUP).toString();
      roster->removeGroup(groupName);
    }
  }
}

void RosterChanger::onRemoveGroupItems(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString streamJid = action->data(ADR_STREAM_JID).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    if (roster && roster->isOpen())
    {
      QString groupName = action->data(ADR_GROUP).toString();
      QList<IRosterItem> ritems = roster->groupItems(groupName);
      if (ritems.count()>0 && 
        QMessageBox::question(NULL,tr("Remove contacts"),
        tr("You are assured that wish to remove %1 contact(s) from roster?").arg(ritems.count()),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
      {
        roster->removeItems(ritems);
      }
    }
  }
}

void RosterChanger::onRosterClosed(IRoster *ARoster)
{
  FAutoSubscriptions.remove(ARoster->streamJid());
}

void RosterChanger::onNotificationActivated(int ANotifyId)
{
  if (FNotifyDialog.contains(ANotifyId))
  {
    SubscriptionDialog *dialog = FNotifyDialog.value(ANotifyId);
    if (dialog)
      dialog->show();
    FNotifications->removeNotification(ANotifyId);
  }
}

void RosterChanger::onNotificationRemoved(int ANotifyId)
{
  if (FNotifyDialog.contains(ANotifyId))
  {
    SubscriptionDialog *dialog = FNotifyDialog.take(ANotifyId);
    if (dialog && !dialog->isVisible())
      dialog->reject();
  }
}

void RosterChanger::onSubscriptionDialogDestroyed()
{
  SubscriptionDialog *dialog = static_cast<SubscriptionDialog *>(sender());
  if (dialog)
  {
    int notifyId = FNotifyDialog.key(dialog);
    FNotifications->removeNotification(notifyId);
  }
}

void RosterChanger::onMultiUserContextMenu(IMultiUserChatWindow * /*AWindow*/, IMultiUser *AUser, Menu *AMenu)
{
  if (!AUser->data(MUDR_REAL_JID).toString().isEmpty())
  {
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AUser->data(MUDR_STREAM_JID).toString()) : NULL;
    if (roster && !roster->rosterItem(AUser->data(MUDR_REAL_JID).toString()).isValid)
    {
      Action *action = new Action(AMenu);
      action->setText(tr("Add contact"));
      action->setData(ADR_STREAM_JID,AUser->data(MUDR_STREAM_JID));
      action->setData(ADR_CONTACT_JID,AUser->data(MUDR_REAL_JID));
      action->setData(ADR_NICK,AUser->data(MUDR_NICK_NAME));
      action->setIcon(RSR_STORAGE_MENUICONS,MNI_RCHANGER_ADD_CONTACT);
      connect(action,SIGNAL(triggered(bool)),SLOT(onShowAddContactDialog(bool)));
      AMenu->addAction(action,AG_MUCM_ROSTERCHANGER,true);
    }
  }
}

Q_EXPORT_PLUGIN2(plg_rosterchanger, RosterChanger)
