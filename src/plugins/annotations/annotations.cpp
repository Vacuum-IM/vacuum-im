#include "annotations.h"

#define PST_ANNOTATIONS       "storage"
#define PSN_ANNOTATIONS       "storage:rosternotes"

#define ADR_STREAMJID         Action::DR_StreamJid
#define ADR_CONTACTJID        Action::DR_Parametr1

Annotations::Annotations()
{
  FPrivateStorage = NULL;
  FRosterSearch = NULL;
  FRosterPlugin = NULL;
  FRostersModel = NULL;
  FRostersViewPlugin = NULL;
}

Annotations::~Annotations()
{

}

void Annotations::pluginInfo(IPluginInfo *APluginInfo)
{
  APluginInfo->name = tr("Annotations"); 
  APluginInfo->description = tr("Allows to add comments to the contacts in the roster");
  APluginInfo->version = "1.0";
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->dependences.append(PRIVATESTORAGE_UUID);
}

bool Annotations::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  IPlugin *plugin = APluginManager->pluginInterface("IPrivateStorage").value(0,NULL);
  if (plugin)
  {
    FPrivateStorage = qobject_cast<IPrivateStorage *>(plugin->instance());
    if (FPrivateStorage)
    {
      connect(FPrivateStorage->instance(),SIGNAL(storageOpened(const Jid &)),SLOT(onPrivateStorageOpened(const Jid &)));
      connect(FPrivateStorage->instance(),SIGNAL(dataSaved(const QString &, const Jid &, const QDomElement &)),
        SLOT(onPrivateDataSaved(const QString &, const Jid &, const QDomElement &)));
      connect(FPrivateStorage->instance(),SIGNAL(dataLoaded(const QString &, const Jid &, const QDomElement &)),
        SLOT(onPrivateDataLoaded(const QString &, const Jid &, const QDomElement &)));
      connect(FPrivateStorage->instance(),SIGNAL(dataError(const QString &, const QString &)),
        SLOT(onPrivateDataError(const QString &, const QString &)));
      connect(FPrivateStorage->instance(),SIGNAL(storageClosed(const Jid &)),SLOT(onPrivateStorageClosed(const Jid &)));
    }
  }

  plugin = APluginManager->pluginInterface("IRosterPlugin").value(0,NULL);
  if (plugin)
  {
    FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());
    if (FRosterPlugin)
    {
      connect(FRosterPlugin->instance(),SIGNAL(rosterItemRemoved(IRoster *, const IRosterItem &)),
        SLOT(onRosterItemRemoved(IRoster *, const IRosterItem &)));
    }
  }

  plugin = APluginManager->pluginInterface("IRostersModel").value(0,NULL);
  if (plugin)
    FRostersModel = qobject_cast<IRostersModel *>(plugin->instance());

  plugin = APluginManager->pluginInterface("IRostersViewPlugin").value(0,NULL);
  if (plugin)
    FRostersViewPlugin = qobject_cast<IRostersViewPlugin *>(plugin->instance());

  plugin = APluginManager->pluginInterface("IRosterSearch").value(0,NULL);
  if (plugin)
    FRosterSearch = qobject_cast<IRosterSearch *>(plugin->instance());

  return FPrivateStorage!=NULL;
}

bool Annotations::initObjects()
{
  if (FRostersViewPlugin)
  {
    IRostersView *rostersView = FRostersViewPlugin->rostersView();
    connect(rostersView->instance(),SIGNAL(indexContextMenu(IRosterIndex *, Menu *)),SLOT(onRosterIndexContextMenu(IRosterIndex *, Menu *)));
    connect(rostersView->instance(),SIGNAL(labelToolTips(IRosterIndex *, int , QMultiMap<int,QString> &)),
      SLOT(onRosterLabelToolTips(IRosterIndex *, int , QMultiMap<int,QString> &)));
  }
  if (FRostersModel)
  {
    FRostersModel->insertDefaultDataHolder(this);
  }
  if (FRosterSearch)
  {
    FRosterSearch->insertSearchField(RDR_ANNOTATIONS,tr("Annotation"),true);
  }
  return true;
}

int Annotations::rosterDataOrder() const
{
  return RDHO_DEFAULT;
}

QList<int> Annotations::rosterDataRoles() const
{
  static const QList<int> dataRoles = QList<int>() << RDR_ANNOTATIONS;
  return dataRoles;
}

QList<int> Annotations::rosterDataTypes() const
{
  static const QList<int> dataTypes = QList<int>() << RIT_CONTACT << RIT_AGENT;
  return dataTypes;
}

QVariant Annotations::rosterData(const IRosterIndex *AIndex, int ARole) const
{
  if (ARole == RDR_ANNOTATIONS)
  {
    QString note = annotation(AIndex->data(RDR_STREAM_JID).toString(),AIndex->data(RDR_BARE_JID).toString());
    return !note.isEmpty() ? QVariant(note) : QVariant();
  }
  return QVariant();
}

bool Annotations::setRosterData(IRosterIndex *AIndex, int ARole, const QVariant &AValue)
{
  if (rosterDataTypes().contains(AIndex->type()) && ARole==RDR_ANNOTATIONS)
  {
    setAnnotation(AIndex->data(RDR_STREAM_JID).toString(),AIndex->data(RDR_BARE_JID).toString(),AValue.toString());
    saveAnnotations(AIndex->data(RDR_STREAM_JID).toString());
    return true;
  }
  return false;
}

bool Annotations::isEnabled(const Jid &AStreamJid) const
{
  return FAnnotations.contains(AStreamJid);
}

QList<Jid> Annotations::annotations(const Jid &AStreamJid) const
{
  return FAnnotations.value(AStreamJid).keys();
}

QString Annotations::annotation(const Jid &AStreamJid, const Jid &AContactJid) const
{
  return FAnnotations.value(AStreamJid).value(AContactJid.bare()).note;
}

QDateTime Annotations::annotationCreateDate(const Jid &AStreamJid, const Jid &AContactJid) const
{
  return FAnnotations.value(AStreamJid).value(AContactJid.bare()).created.toLocal();
}

QDateTime Annotations::annotationModifyDate(const Jid &AStreamJid, const Jid &AContactJid) const
{
  return FAnnotations.value(AStreamJid).value(AContactJid.bare()).modified.toLocal();
}

void Annotations::setAnnotation(const Jid &AStreamJid, const Jid &AContactJid, const QString &ANote)
{
  if (isEnabled(AStreamJid))
  {
    if (!ANote.isEmpty())
    {
      Annotation &item = FAnnotations[AStreamJid][AContactJid.bare()];
      item.modified = DateTime(QDateTime::currentDateTime());
      if (!item.created.isValid())
        item.created = item.modified;
      item.note = ANote;
    }
    else
    {
      FAnnotations[AStreamJid].remove(AContactJid.bare());
    }
    emit annotationModified(AStreamJid,AContactJid);
    updateDataHolder(AStreamJid,QList<Jid>()<<AContactJid);
  }
}

bool Annotations::loadAnnotations(const Jid &AStreamJid)
{
  if (FPrivateStorage && !FLoadRequests.contains(AStreamJid))
  {
    QString id = FPrivateStorage->loadData(AStreamJid,PST_ANNOTATIONS,PSN_ANNOTATIONS);
    if (!id.isEmpty())
    {
      FLoadRequests.insert(AStreamJid,id);
      return true;
    }
  }
  return false;
}

bool Annotations::saveAnnotations(const Jid &AStreamJid)
{
  if (isEnabled(AStreamJid) && !FSaveRequests.contains(AStreamJid))
  {
    QDomDocument doc;
    QDomElement storage = doc.appendChild(doc.createElementNS(PSN_ANNOTATIONS,PST_ANNOTATIONS)).toElement();

    const QMap<Jid, Annotation> &items = FAnnotations.value(AStreamJid);
    QMap<Jid, Annotation>::const_iterator it = items.constBegin();
    while (it != items.constEnd())
    {
      QDomElement elem = storage.appendChild(doc.createElement("note")).toElement();
      elem.setAttribute("jid",it.key().eBare());
      elem.setAttribute("cdate",it.value().created.toX85UTC());
      elem.setAttribute("mdate",it.value().modified.toX85UTC());
      elem.appendChild(doc.createTextNode(it.value().note));
      it++;
    }

    QString id = FPrivateStorage->saveData(AStreamJid,doc.documentElement());
    if (!id.isEmpty())
    {
      FSaveRequests.insert(AStreamJid,id);
      return true;
    }
  }
  return false;
}

void Annotations::updateDataHolder(const Jid &AStreamJid, const QList<Jid> &AContactJids)
{
  if (FRostersModel && !AContactJids.isEmpty() && FRostersModel->streamRoot(AStreamJid))
  {
    QMultiHash<int,QVariant> findData;
    foreach(Jid contactJid, AContactJids)
      findData.insertMulti(RDR_BARE_JID,contactJid.pBare());
    QList<IRosterIndex *> indexes = FRostersModel->streamRoot(AStreamJid)->findChild(findData,true);
    foreach (IRosterIndex *index, indexes)
      emit rosterDataChanged(index,RDR_ANNOTATIONS);
  }
}

void Annotations::onPrivateStorageOpened(const Jid &AStreamJid)
{
  loadAnnotations(AStreamJid);
}

void Annotations::onPrivateDataSaved(const QString &AId, const Jid &AStreamJid, const QDomElement &/*AElement*/)
{
  if (FSaveRequests.value(AStreamJid) == AId)
  {
    FSaveRequests.remove(AStreamJid);
    emit annotationsSaved(AStreamJid);
  }
}

void Annotations::onPrivateDataLoaded(const QString &AId, const Jid &AStreamJid, const QDomElement &AElement)
{
  if (FLoadRequests.value(AStreamJid) == AId)
  {
    FLoadRequests.remove(AStreamJid);

    QMap<Jid, Annotation> &items = FAnnotations[AStreamJid];
    items.clear();

    QDomElement elem = AElement.firstChildElement("note");
    while (!elem.isNull())
    {
      Jid itemJid = elem.attribute("jid");
      if (itemJid.isValid() && !elem.text().isEmpty())
      {
        Annotation item;
        item.created.setDateTime(elem.attribute("cdate"));
        item.modified.setDateTime(elem.attribute("mdate"));
        item.note = elem.text();
        items.insert(itemJid.bare(),item);
      }
      elem= elem.nextSiblingElement("note");
    }
    emit annotationsLoaded(AStreamJid);
    updateDataHolder(AStreamJid,annotations(AStreamJid));
  }
}

void Annotations::onPrivateDataError(const QString &AId, const QString &AError)
{
  if (FLoadRequests.values().contains(AId))
  {
    Jid streamJid = FLoadRequests.key(AId);
    FLoadRequests.remove(streamJid);
    emit annotationsError(streamJid, AError);
  }
  else if (FSaveRequests.values().contains(AId))
  {
    Jid streamJid = FSaveRequests.key(AId);
    FSaveRequests.remove(streamJid);
    emit annotationsError(streamJid, AError);
  }
}

void Annotations::onPrivateStorageClosed(const Jid &AStreamJid)
{
  QList<Jid> curAnnotations = annotations(AStreamJid);

  qDeleteAll(FEditDialogs.take(AStreamJid));
  FLoadRequests.remove(AStreamJid);
  FSaveRequests.remove(AStreamJid);
  FAnnotations.remove(AStreamJid);

  updateDataHolder(AStreamJid,curAnnotations);
}

void Annotations::onRosterItemRemoved(IRoster *ARoster, const IRosterItem &ARosterItem)
{
  if (isEnabled(ARoster->streamJid()))
  {
    if (!annotation(ARoster->streamJid(),ARosterItem.itemJid).isEmpty())
    {
      setAnnotation(ARoster->streamJid(),ARosterItem.itemJid,"");
      saveAnnotations(ARoster->streamJid());
    }
  }
}

void Annotations::onRosterIndexContextMenu(IRosterIndex *AIndex, Menu *AMenu)
{
  Jid streamJid = AIndex->data(RDR_STREAM_JID).toString();
  Jid contactJid = AIndex->data(RDR_BARE_JID).toString();
  if (rosterDataTypes().contains(AIndex->type()) && isEnabled(streamJid) && contactJid.isValid())
  {
    Action *action = new Action(AMenu);
    action->setText(tr("Annotation"));
    action->setIcon(RSR_STORAGE_MENUICONS,MNI_ANNOTATIONS);
    action->setData(ADR_STREAMJID,streamJid.full());
    action->setData(ADR_CONTACTJID,contactJid.bare());
    connect(action,SIGNAL(triggered(bool)),SLOT(onEditNoteActionTriggered(bool)));
    AMenu->addAction(action,AG_RVCM_ANNOTATIONS,true);
  }
}

void Annotations::onRosterLabelToolTips(IRosterIndex *AIndex, int ALabelId, QMultiMap<int,QString> &AToolTips)
{
  if (ALabelId==RLID_DISPLAY && rosterDataTypes().contains(AIndex->type()))
  {
    QString note = AIndex->data(RDR_ANNOTATIONS).toString();
    if (!note.isEmpty())
      AToolTips.insert(RTTO_ANNOTATIONS,QString("%1 <div style='margin-left:10px;'>%2</div>").arg(tr("Annotation:")).arg(Qt::escape(note).replace("\n","<br>")));
  }
}

void Annotations::onEditNoteActionTriggered(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    Jid streamJid = action->data(ADR_STREAMJID).toString();
    Jid contactJid = action->data(ADR_CONTACTJID).toString();
    EditNoteDialog *dialog = FEditDialogs.value(streamJid).value(contactJid);
    if (!dialog)
    {
      dialog = new EditNoteDialog(this,streamJid,contactJid);
      FEditDialogs[streamJid].insert(contactJid,dialog);
      connect(dialog,SIGNAL(dialogDestroyed()),SLOT(onEditNoteDialogDestroyed()));
    }
    dialog->show();
  }
}

void Annotations::onEditNoteDialogDestroyed()
{
  EditNoteDialog *dialog = qobject_cast<EditNoteDialog *>(sender());
  if (dialog)
    FEditDialogs[dialog->streamJid()].remove(dialog->contactJid());
}

Q_EXPORT_PLUGIN2(plg_annotations, Annotations)