#ifndef ROSTERCHANGER_H
#define ROSTERCHANGER_H

#include <QDateTime>
#include "../../definations/initorders.h"
#include "../../definations/actiongroups.h"
#include "../../definations/rosterlabelorders.h"
#include "../../interfaces/irosterchanger.h"
#include "../../interfaces/ipluginmanager.h"
#include "../../interfaces/irostersmodel.h"
#include "../../interfaces/irostersview.h"
#include "../../interfaces/iroster.h"
#include "../../interfaces/imainwindow.h"
#include "../../interfaces/itraymanager.h"
#include "../../utils/skin.h"
#include "addcontactdialog.h"

struct SubsItem 
{
  Jid streamJid;
  Jid contactJid;
  IRoster::SubsType type;
  QString status;
  QDateTime time;
  int trayId;
};

class RosterChanger : 
  public QObject,
  public IPlugin,
  public IRosterChanger
{
  Q_OBJECT;
  Q_INTERFACES(IPlugin IRosterChanger);

public:
  RosterChanger();
  ~RosterChanger();

  virtual QObject *instance() { return this; }

  //IPlugin
  virtual QUuid pluginUuid() const { return ROSTERCHANGER_UUID; }
  virtual void pluginInfo(PluginInfo *APluginInfo);
  virtual bool initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/);
  virtual bool initObjects();
  virtual bool initSettings() { return true; }
  virtual bool startPlugin() { return true; }

  //IRosterChanger
  virtual void showAddContactDialog(const Jid &AStreamJid, const Jid &AJid, const QString &ANick,
    const QString &AGroup, const QString &ARequest) const;
  virtual Menu *addContactMenu() const { return FAddContactMenu; }
public slots:
  virtual void showAddContactDialogByAction(bool);
protected:
  Menu *createGroupMenu(const QHash<int,QVariant> AData, const QSet<QString> &AExceptGroups, 
    bool ANewGroup, bool ARootGroup, const char *ASlot, Menu *AParent);
protected:
  IRosterIndexList getContactIndexList(const Jid &AStreamJid, const Jid &AJid);
protected slots:
  void onRostersViewContextMenu(IRosterIndex *AIndex, Menu *AMenu);
  void onTrayContextMenu(int ANotifyId, Menu *AMenu);
  //Operations on subscription
  void onSendSubscription(bool);
  void onReceiveSubscription(IRoster *ARoster, const Jid &AFromJid, 
    IRoster::SubsType AType, const QString &AStatus);
  //Operations on items
  void onRenameItem(bool);
  void onCopyItemToGroup(bool);
  void onMoveItemToGroup(bool);
  void onRemoveItemFromGroup(bool);
  void onRemoveItemFromRoster(bool);
  //Operations on group
  void onRenameGroup(bool);
  void onCopyGroupToGroup(bool);
  void onMoveGroupToGroup(bool);
  void onRemoveGroup(bool);
  //Operations on stream
  void onAddContact(AddContactDialog *ADialog);
protected slots:
  void onRosterOpened(IRoster *ARoster);
  void onRosterClosed(IRoster *ARoster);
  void onRosterLabelDClicked(IRosterIndex *AIndex, int ALabelId, bool &AAccepted);
  void onRosterLabelToolTips(IRosterIndex *AIndex, int ALabelId, QMultiMap<int,QString> &AToolTips);
  void onTrayNotifyActivated(int ANotifyId);
private:
  IRosterPlugin *FRosterPlugin;
  IRostersModelPlugin *FRostersModelPlugin;
  IRostersModel *FRostersModel;
  IRostersViewPlugin *FRostersViewPlugin;
  IRostersView *FRostersView;
  IMainWindowPlugin *FMainWindowPlugin;
  ITrayManager *FTrayManager;
private:
  Menu *FAddContactMenu;
  SkinIconset FSystemIconset;
private:
  int FSubsLabelId;
  QHash<IRoster *,Action *> FActions;
  QHash<Jid,QList<SubsItem *> > FSubsItems;
  QHash<int,Jid> FTrayIdToJid;
};

#endif // ROSTERCHANGER_H
