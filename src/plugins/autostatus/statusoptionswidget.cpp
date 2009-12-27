#include "statusoptionswidget.h"

#include <QHeaderView>
#include <QComboBox>
#include <QTimeEdit>
#include <QLineEdit>

#define SDR_VALUE     Qt::UserRole

#define COL_ENABLED   0
#define COL_TIME      1
#define COL_SHOW      2
#define COL_TEXT      3

Delegate::Delegate(IStatusChanger *AStatusChanger, QObject *AParent) : QItemDelegate(AParent)
{
  FStatusChanger = AStatusChanger;
}

QWidget *Delegate::createEditor(QWidget *AParent, const QStyleOptionViewItem &AOption, const QModelIndex &AIndex) const
{
  switch(AIndex.column())
  {
  case COL_ENABLED: 
    {
      return NULL;
    }
  case COL_TIME: 
    {
      QTimeEdit *timeEdit = new QTimeEdit(AParent);
      timeEdit->setDisplayFormat("HH:mm:ss");
      return timeEdit;
    }
  case COL_SHOW:
    {
      QComboBox *comboBox = new QComboBox(AParent);
      comboBox->addItem(FStatusChanger->iconByShow(IPresence::Away),FStatusChanger->nameByShow(IPresence::Away),IPresence::Away);
      comboBox->addItem(FStatusChanger->iconByShow(IPresence::DoNotDisturb),FStatusChanger->nameByShow(IPresence::DoNotDisturb),IPresence::DoNotDisturb);
      comboBox->addItem(FStatusChanger->iconByShow(IPresence::ExtendedAway),FStatusChanger->nameByShow(IPresence::ExtendedAway),IPresence::ExtendedAway);
      comboBox->addItem(FStatusChanger->iconByShow(IPresence::Invisible),FStatusChanger->nameByShow(IPresence::Invisible),IPresence::Invisible);
      comboBox->addItem(FStatusChanger->iconByShow(IPresence::Online),FStatusChanger->nameByShow(IPresence::Online),IPresence::Online);
      comboBox->addItem(FStatusChanger->iconByShow(IPresence::Chat),FStatusChanger->nameByShow(IPresence::Chat),IPresence::Chat);
      comboBox->addItem(FStatusChanger->iconByShow(IPresence::Offline),FStatusChanger->nameByShow(IPresence::Offline),IPresence::Offline);
      comboBox->setEditable(false);
      return comboBox;
    }
  default: 
    return QItemDelegate::createEditor(AParent,AOption,AIndex);
  }
}

void Delegate::setEditorData(QWidget *AEditor, const QModelIndex &AIndex) const
{
  switch(AIndex.column())
  {
  case COL_TIME: 
    {
      QTimeEdit *timeEdit = qobject_cast<QTimeEdit *>(AEditor);
      if (timeEdit)
        timeEdit->setTime(QTime(0,0).addSecs(AIndex.data(SDR_VALUE).toInt()));
    }
  case COL_SHOW:
    {
      QComboBox *comboBox = qobject_cast<QComboBox *>(AEditor);
      if (comboBox)
        comboBox->setCurrentIndex(comboBox->findData(AIndex.data(SDR_VALUE).toInt()));
      break;
    }
  default: 
    QItemDelegate::setEditorData(AEditor,AIndex);
  }
}

void Delegate::setModelData(QWidget *AEditor, QAbstractItemModel *AModel, const QModelIndex &AIndex) const
{
  switch(AIndex.column())
  {
  case COL_TIME:
    {
      QTimeEdit *timeEdit = qobject_cast<QTimeEdit *>(AEditor);
      if (timeEdit)
      {
        AModel->setData(AIndex,QTime(0,0).secsTo(timeEdit->time()),SDR_VALUE);
        AModel->setData(AIndex,timeEdit->time().toString(),Qt::DisplayRole);
      }
      break;
    }
  case COL_SHOW: 
    {
      QComboBox *comboBox = qobject_cast<QComboBox *>(AEditor);
      if (comboBox)
      {
        int show = comboBox->itemData(comboBox->currentIndex()).toInt();
        AModel->setData(AIndex, FStatusChanger->iconByShow(show), Qt::DecorationRole);
        AModel->setData(AIndex, FStatusChanger->nameByShow(show), Qt::DisplayRole);
        AModel->setData(AIndex, show, SDR_VALUE);
      }
      break;
    }
  case COL_TEXT:
    {
      QLineEdit *lineEdit = qobject_cast<QLineEdit *>(AEditor);
      if (lineEdit)
      {
        AModel->setData(AIndex, lineEdit->text(), Qt::DisplayRole);
        AModel->setData(AIndex, lineEdit->text(), SDR_VALUE);
      }
      break;
    }
  default: 
    QItemDelegate::setModelData(AEditor,AModel,AIndex);
  }
}

void Delegate::updateEditorGeometry(QWidget *AEditor, const QStyleOptionViewItem &AOption, const QModelIndex &AIndex) const
{
  switch(AIndex.column())
  {
  case COL_TIME: 
    {
      AEditor->setGeometry(AOption.rect);
      AEditor->setMinimumWidth(AEditor->sizeHint().width());
      break;
    }
  case COL_SHOW: 
    {
      AEditor->setGeometry(AOption.rect);
     break;
    }
  default:
    QItemDelegate::updateEditorGeometry(AEditor,AOption,AIndex);
  }
}

StatusOptionsWidget::StatusOptionsWidget(IAutoStatus *AAutoStatus, IStatusChanger *AStatusChanger, QWidget *AParent) : QWidget(AParent)
{
  ui.setupUi(this);
  FAutoStatus = AAutoStatus;
  FStatusChanger = AStatusChanger;

  ui.tbwRules->setItemDelegate(new Delegate(FStatusChanger,ui.tbwRules));

  ui.tbwRules->setColumnCount(4);
  ui.tbwRules->setHorizontalHeaderLabels(QStringList() << "" << tr("Time") << tr("Status") << tr("Text"));

  foreach(int ruleId, FAutoStatus->rules())
  {
    appendTableRow(FAutoStatus->ruleValue(ruleId),ruleId);
  }

  ui.tbwRules->sortItems(COL_TIME);
  ui.tbwRules->horizontalHeader()->setResizeMode(COL_ENABLED,QHeaderView::ResizeToContents);
  ui.tbwRules->horizontalHeader()->setResizeMode(COL_TIME,QHeaderView::ResizeToContents);
  ui.tbwRules->horizontalHeader()->setResizeMode(COL_SHOW,QHeaderView::ResizeToContents);
  ui.tbwRules->horizontalHeader()->setResizeMode(COL_TEXT,QHeaderView::Stretch);
  ui.tbwRules->horizontalHeader()->setSortIndicatorShown(false);
  ui.tbwRules->verticalHeader()->hide();

  connect(ui.pbtAdd,SIGNAL(clicked(bool)),SLOT(onAddButtonClicked(bool)));
  connect(ui.pbtDelete,SIGNAL(clicked(bool)),SLOT(onDeleteButtonClicked(bool)));
}

StatusOptionsWidget::~StatusOptionsWidget()
{

}

void StatusOptionsWidget::apply()
{
  QList<int> oldRules = FAutoStatus->rules();
  for (int i = 0; i<ui.tbwRules->rowCount(); i++)
  {
    
    IAutoStatusRule rule;
    rule.time = ui.tbwRules->item(i,COL_TIME)->data(SDR_VALUE).toInt();
    rule.show = ui.tbwRules->item(i,COL_SHOW)->data(SDR_VALUE).toInt();
    rule.text = ui.tbwRules->item(i,COL_TEXT)->data(SDR_VALUE).toString();

    int ruleId = ui.tbwRules->item(i,COL_ENABLED)->data(SDR_VALUE).toInt();
    if (ruleId > 0)
    {
      IAutoStatusRule oldRule = FAutoStatus->ruleValue(ruleId);
      if (oldRule.time!=rule.time || 
          oldRule.show!=rule.show || 
          oldRule.text!=rule.text)
        FAutoStatus->updateRule(ruleId,rule);
      oldRules.removeAt(oldRules.indexOf(ruleId));
    }
    else
    {
      ruleId = FAutoStatus->insertRule(rule);
      ui.tbwRules->item(i,COL_ENABLED)->setData(SDR_VALUE,ruleId);
    }
    FAutoStatus->setRuleEnabled(ruleId,ui.tbwRules->item(i,COL_ENABLED)->checkState()==Qt::Checked);
  }

  foreach(int ruleId, oldRules)
    FAutoStatus->removeRule(ruleId);

  emit optionsAccepted();
}

int StatusOptionsWidget::appendTableRow(const IAutoStatusRule &ARule, int ARuleId)
{
  QTableWidgetItem *enabled = new QTableWidgetItem;
  enabled->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);
  enabled->setCheckState(FAutoStatus->isRuleEnabled(ARuleId) ? Qt::Checked : Qt::Unchecked);
  enabled->setData(SDR_VALUE,ARuleId);

  QTableWidgetItem *time = new QTableWidgetItem(QTime(0,0).addSecs(ARule.time).toString());
  time->setData(SDR_VALUE,ARule.time);

  QTableWidgetItem *status = new QTableWidgetItem(FStatusChanger->iconByShow(ARule.show),FStatusChanger->nameByShow(ARule.show));
  status->setData(SDR_VALUE,ARule.show);

  QTableWidgetItem *text = new QTableWidgetItem(ARule.text);
  text->setData(SDR_VALUE,ARule.text);

  int curRow = ui.tbwRules->rowCount();
  ui.tbwRules->setRowCount(curRow+1);
  ui.tbwRules->setItem(curRow,COL_ENABLED,enabled);
  ui.tbwRules->setItem(enabled->row(),COL_TIME,time);
  ui.tbwRules->setItem(enabled->row(),COL_SHOW,status);
  ui.tbwRules->setItem(enabled->row(),COL_TEXT,text);

  return enabled->row();
}

void StatusOptionsWidget::onAddButtonClicked(bool)
{
  IAutoStatusRule rule;
  if (ui.tbwRules->rowCount()>0)
    rule.time = ui.tbwRules->item(ui.tbwRules->rowCount()-1,COL_TIME)->data(SDR_VALUE).toInt() + 5*60;
  else
    rule.time = 10*60;
  rule.show = IPresence::Away;
  rule.text = tr("Auto status: away");
  ui.tbwRules->setCurrentCell(appendTableRow(rule,-1),COL_ENABLED);
  ui.tbwRules->horizontalHeader()->doItemsLayout();
}

void StatusOptionsWidget::onDeleteButtonClicked(bool)
{
  QTableWidgetItem *item = ui.tbwRules->currentItem();
  if (item)
    ui.tbwRules->removeRow(item->row());
}
