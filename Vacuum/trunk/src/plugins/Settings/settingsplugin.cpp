#include <QtDebug>
#include "settingsplugin.h"

#include <QTextStream>
#include <QByteArray>
#include <QVBoxLayout>
#include "../../definations/actiongroups.h"

SettingsPlugin::SettingsPlugin()
{
  actOpenOptionsDialog = NULL;
  FProfileOpened = false;
  FFile.setParent(this);
  FSystemIconset.openFile(SYSTEM_ICONSETFILE);
}

SettingsPlugin::~SettingsPlugin()
{
  onPluginManagerQuit();
  qDeleteAll(FNodes);
  FCleanupHandler.clear(); 
}

void SettingsPlugin::pluginInfo(PluginInfo *APluginInfo)
{
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->description = tr("Managing profiles and settings");
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->name = tr("Settings Manager"); 
  APluginInfo->uid = SETTINGS_UUID;
  APluginInfo ->version = "0.1";
}

bool SettingsPlugin::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  FPluginManager = APluginManager;
  connect(FPluginManager->instance(),SIGNAL(aboutToQuit()),SLOT(onPluginManagerQuit()));
  
  IPlugin *plugin = FPluginManager->getPlugins("IMainWindowPlugin").value(0,NULL);
  if (plugin)
  {
    FMainWindowPlugin = qobject_cast<IMainWindowPlugin *>(plugin->instance());
    if (FMainWindowPlugin)
    {
      connect(FMainWindowPlugin->instance(),SIGNAL(mainWindowCreated(IMainWindow *)),
        SLOT(onMainWindowCreated(IMainWindow *)));
    }
  }
  return setSettingsFile("config.xml");
}

bool SettingsPlugin::initSettings()
{
  setProfile(QString::null);
  return true;
}

//ISettingsPlugin
ISettings *SettingsPlugin::openSettings(const QUuid &APluginId, QObject *AParent)
{
  Settings *settings = new Settings(APluginId,this,AParent);
  FCleanupHandler.add(settings); 
  return settings;
}

bool SettingsPlugin::setSettingsFile(const QString &AFileName)
{
  if (FFile.isOpen())
    FFile.close(); 

  if (FProfileOpened)
  {
    FProfileOpened = false;
    emit profileClosed();
  }

  FFile.setFileName(AFileName);

  if (!FFile.exists())
  {
    if (FFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      FFile.close(); 
      return true;
    }
  }
  else if (FFile.open(QIODevice::ReadOnly))
  {
    if (FFile.size() == 0 || FSettings.setContent(FFile.readAll(),true))
    {
      FFile.close();
      return true;
    }
    FFile.close();
  }
  return false;
}

bool SettingsPlugin::saveSettings()
{
  if (FFile.isOpen())
    FFile.close(); 

  if (FFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    FFile.write(FSettings.toByteArray());  
    FFile.close(); 
    return true;
  }
  return false;
}

QDomElement SettingsPlugin::setProfile(const QString &AProfile)
{
  if (FProfileOpened)
  {
    FProfileOpened = false;
    emit profileClosed();
  }

  if (!AProfile.isEmpty()) 
    FProfile = profileNode(AProfile);
  else
    FProfile = profileNode(FSettings.documentElement().attribute("profile","Default"));

  if (!FProfile.isNull())
  {
    FProfileOpened = true;
    FSettings.documentElement().setAttribute("profile",FProfile.attribute("name","Default"));
    emit profileOpened();
  }
  return FProfile;
}

QDomElement SettingsPlugin::profileNode(const QString &AProfile) 
{
  if (AProfile.isEmpty())
    return QDomElement();

  if (FSettings.isNull())
  {
    QDomElement config = FSettings.appendChild(FSettings.createElement("config")).toElement();
    config.setAttribute("Application","Vacuum");
    config.setAttribute("version","1.0");
    config.setAttribute("profile","Default");  
  }
  
  QDomElement profile = FSettings.documentElement().firstChildElement("profile");  
  while (!profile.isNull() && profile.attribute("name","Default") != AProfile)
    profile = profile.nextSiblingElement("profile"); 
  
  if (profile.isNull())
  {
    profile = FSettings.documentElement().appendChild(FSettings.createElement("profile")).toElement();
    profile.setAttribute("name",AProfile); 
  }

  return profile;
}

QDomElement SettingsPlugin::pluginNode(const QUuid &AId) 
{
  if (FProfile.isNull())
    return QDomElement();

  QDomElement node = FProfile.firstChildElement("plugin");  
  while (!node.isNull() && node.attribute("uid") != AId)
    node = node.nextSiblingElement("plugin");

  if (node.isNull())
  {
    node = FProfile.appendChild(FSettings.createElement("plugin")).toElement();
    node.setAttribute("uid",AId.toString()); 
  }

  return node;
}

void SettingsPlugin::openOptionsNode(const QString &ANode, const QString &AName,  
                                     const QString &ADescription, const QIcon &AIcon)
{
  OptionsNode *node = FNodes.value(ANode,NULL);
  if (!node)
  {
    node = new OptionsNode;
    node->name = AName;
    node->desc = ADescription;
    node->icon = AIcon;
    FNodes.insert(ANode,node);
    if (!FOptionsDialog.isNull())
      FOptionsDialog->openNode(ANode,AName,ADescription,AIcon,createNodeWidget(ANode));
    emit optionsNodeOpened(ANode);
  }
  else
  {
    if (!AName.isEmpty())
      node->name = AName;
    if (!ADescription.isEmpty())
      node->desc = ADescription;
    if (!AIcon.isNull())
      node->icon = AIcon;
  }
}

void SettingsPlugin::closeOptionsNode(const QString &ANode)
{
  OptionsNode *node = FNodes.value(ANode,NULL);
  if (node)
  {
    emit optionsNodeClosed(ANode);
    if (!FOptionsDialog.isNull())
      FOptionsDialog->closeNode(ANode);
    FNodes.remove(ANode);
    delete node;
  }
}

void SettingsPlugin::appendOptionsHolder(IOptionsHolder *AOptionsHolder)
{
  if (!FOptionsHolders.contains(AOptionsHolder))
  {
    FOptionsHolders.append(AOptionsHolder);
    emit optionsHolderAdded(AOptionsHolder);
  }
}

void SettingsPlugin::removeOptionsHolder(IOptionsHolder *AOptionsHolder)
{
  if (!FOptionsHolders.contains(AOptionsHolder))
  {
    emit optionsHolderRemoved(AOptionsHolder);
    FOptionsHolders.removeAt(FOptionsHolders.indexOf(AOptionsHolder));
  }
}

void SettingsPlugin::openOptionsDialog(const QString &ANode)
{
  if (FOptionsDialog.isNull())
  {
    FOptionsDialog = new OptionsDialog;
    FOptionsDialog->setWindowIcon(FSystemIconset.iconByName("psi/options"));
    connect(FOptionsDialog, SIGNAL(accepted()),SLOT(onOptionsDialogAccepted()));
    connect(FOptionsDialog, SIGNAL(rejected()),SLOT(onOptionsDialogRejected()));

    QMap<QString, OptionsNode *>::const_iterator it = FNodes.constBegin();
    while (it != FNodes.constEnd())
    {
      FOptionsDialog->openNode(it.key(),it.value()->name,it.value()->desc,it.value()->icon,createNodeWidget(it.key()));
      it++;
    }
  }
  if (!ANode.isEmpty())
    FOptionsDialog->showNode(ANode);
  FOptionsDialog->show();
}

void SettingsPlugin::openOptionsDialogAction(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    QString node = action->data(Action::DR_Parametr1).toString();
    openOptionsDialog(node);
  }
}

QWidget *SettingsPlugin::createNodeWidget(const QString &ANode)
{
  QWidget *nodeWidget = new QWidget;
  QVBoxLayout *nodeLayout = new QVBoxLayout;
  nodeLayout->setMargin(6);
  nodeLayout->setSpacing(3);
  nodeWidget->setLayout(nodeLayout);
  
  QMap<int, QWidget *> widgetsByOrder;
  IOptionsHolder *optionsHolder;
  foreach(optionsHolder,FOptionsHolders)
  {
    int order = 500;
    QWidget *itemWidget = optionsHolder->optionsWidget(ANode,order);
    if (itemWidget)
      widgetsByOrder.insert(order,itemWidget);
  }
  
  QWidget *itemWidget;
  foreach(itemWidget,widgetsByOrder)
    nodeLayout->addWidget(itemWidget);
  nodeLayout->addStretch();
  
  return nodeWidget;
}

void SettingsPlugin::onMainWindowCreated(IMainWindow *AMainWindow)
{
  actOpenOptionsDialog = new Action(this);
  actOpenOptionsDialog->setIcon(SYSTEM_ICONSETFILE,"psi/options");
  actOpenOptionsDialog->setText(tr("Options..."));
  connect(actOpenOptionsDialog,SIGNAL(triggered(bool)),SLOT(openOptionsDialogAction(bool)));
  AMainWindow->mainMenu()->addAction(actOpenOptionsDialog,SETTINGS_ACTION_GROUP_OPTIONS,true);
}

void SettingsPlugin::onOptionsDialogAccepted()
{
  emit optionsDialogAccepted();
  saveSettings();
}

void SettingsPlugin::onOptionsDialogRejected()
{
  emit optionsDialogRejected();
}

void SettingsPlugin::onPluginManagerQuit()
{
  if (!FOptionsDialog.isNull())
    FOptionsDialog->reject();

  if (FProfileOpened)
  {
    emit profileClosed();
    saveSettings();
    FProfileOpened = false;
  }
}

Q_EXPORT_PLUGIN2(SettingsPlugin, SettingsPlugin)
