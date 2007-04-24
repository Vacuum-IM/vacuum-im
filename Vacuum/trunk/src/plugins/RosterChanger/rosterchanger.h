#ifndef ROSTERCHANGER_H
#define ROSTERCHANGER_H

#include "../../interfaces/ipluginmanager.h"
#include "../../interfaces/irostersview.h"
#include "../../interfaces/iroster.h"
#include "../../utils/menu.h"

#define ROSTERCHANGER_UUID "{018E7891-2743-4155-8A70-EAB430573500}"

class RosterChanger : 
  public QObject,
  public IPlugin
{
  Q_OBJECT;
  Q_INTERFACES(IPlugin);

public:
  RosterChanger();
  ~RosterChanger();

  virtual QObject *instance() { return this; }

  //IPlugin
  virtual QUuid pluginUuid() const { return ROSTERCHANGER_UUID; }
  virtual void pluginInfo(PluginInfo *APluginInfo);
  virtual bool initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/);
  virtual bool initObjects() { return true; }
  virtual bool initSettings() { return true; }
  virtual bool startPlugin() { return true; }

  //IRosterChanger
protected:
  Menu *createGroupMenu(const QHash<int,QVariant> AData, const QSet<QString> &AExceptGroups, 
    bool ANewGroup, bool ARootGroup, const char *ASlot, Menu *AParent);
protected slots:
  void onRostersViewCreated(IRostersView *ARostersView);
  void onRostersViewContextMenu(const QModelIndex &AIndex, Menu *AMenu);
  //Operations on subscription
  void onSubscription(bool);
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
private:
  IRosterPlugin *FRosterPlugin;
  IRostersViewPlugin *FRostersViewPlugin;
};

#endif // ROSTERCHANGER_H
