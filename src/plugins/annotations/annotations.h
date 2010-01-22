#ifndef ANNOTATIONS_H
#define ANNOTATIONS_H

#include <QDomDocument>
#include <definations/actiongroups.h>
#include <definations/rosterlabelorders.h>
#include <definations/rosterindextyperole.h>
#include <definations/rosterdataholderorders.h>
#include <definations/rostertooltiporders.h>
#include <definations/menuicons.h>
#include <definations/resources.h>
#include <interfaces/ipluginmanager.h>
#include <interfaces/iannotations.h>
#include <interfaces/iroster.h>
#include <interfaces/irostersearch.h>
#include <interfaces/iprivatestorage.h>
#include <interfaces/irostersview.h>
#include <interfaces/irostersmodel.h>
#include <utils/datetime.h>
#include "editnotedialog.h"

struct Annotation {
  DateTime created;
  DateTime modified;
  QString note;
};

class Annotations : 
  public QObject,
  public IPlugin,
  public IAnnotations,
  public IRosterDataHolder
{
  Q_OBJECT;
  Q_INTERFACES(IPlugin IAnnotations IRosterDataHolder);
public:
  Annotations();
  ~Annotations();
  //IPlugin
  virtual QObject *instance() { return this; }
  virtual QUuid pluginUuid() const { return ANNOTATIONS_UUID; }
  virtual void pluginInfo(IPluginInfo *APluginInfo);
  virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
  virtual bool initObjects();
  virtual bool initSettings() { return true; }
  virtual bool startPlugin() { return true; }
  //IRosterDataHolder
  virtual int rosterDataOrder() const;
  virtual QList<int> rosterDataRoles() const;
  virtual QList<int> rosterDataTypes() const;
  virtual QVariant rosterData(const IRosterIndex *AIndex, int ARole) const;
  virtual bool setRosterData(IRosterIndex *AIndex, int ARole, const QVariant &AValue);
  //IAnnotations
  virtual bool isEnabled(const Jid &AStreamJid) const;
  virtual QList<Jid> annotations(const Jid &AStreamJid) const;
  virtual QString annotation(const Jid &AStreamJid, const Jid &AContactJid) const;
  virtual QDateTime annotationCreateDate(const Jid &AStreamJid, const Jid &AContactJid) const;
  virtual QDateTime annotationModifyDate(const Jid &AStreamJid, const Jid &AContactJid) const;
  virtual void setAnnotation(const Jid &AStreamJid, const Jid &AContactJid, const QString &ANote);
  virtual bool loadAnnotations(const Jid &AStreamJid);
  virtual bool saveAnnotations(const Jid &AStreamJid);
signals:
  void annotationsLoaded(const Jid &AStreamJid);
  void annotationsSaved(const Jid &AStreamJid);
  void annotationsError(const Jid &AStreamJid, const QString &AError);
  void annotationModified(const Jid &AStreamJid, const Jid &AContactJid);
  void rosterDataChanged(IRosterIndex *AIndex = NULL, int ARole = 0);
protected:
  void updateDataHolder(const Jid &AStreamJid, const QList<Jid> &AContactJids);
protected slots:
  void onPrivateStorageOpened(const Jid &AStreamJid);
  void onPrivateDataSaved(const QString &AId, const Jid &AStreamJid, const QDomElement &AElement);
  void onPrivateDataLoaded(const QString &AId, const Jid &AStreamJid, const QDomElement &AElement);
  void onPrivateDataError(const QString &AId, const QString &AError);
  void onPrivateStorageClosed(const Jid &AStreamJid);
  void onRosterItemRemoved(IRoster *ARoster, const IRosterItem &ARosterItem);
  void onRosterIndexContextMenu(IRosterIndex *AIndex, Menu *AMenu);
  void onRosterLabelToolTips(IRosterIndex *AIndex, int ALabelId, QMultiMap<int,QString> &AToolTips);
  void onEditNoteActionTriggered(bool);
  void onEditNoteDialogDestroyed();
private:
  IPrivateStorage *FPrivateStorage;
  IRosterSearch *FRosterSearch;
  IRosterPlugin *FRosterPlugin;
  IRostersModel *FRostersModel;
  IRostersViewPlugin *FRostersViewPlugin;
private:
  QMap<Jid, QString> FLoadRequests;
  QMap<Jid, QString> FSaveRequests;
  QMap<Jid, QMap<Jid, Annotation> > FAnnotations;
  QMap<Jid, QMap<Jid, EditNoteDialog *> > FEditDialogs;
};

#endif // ANNOTATIONS_H