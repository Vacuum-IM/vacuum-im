#include <QtDebug>
#include "mainwindow.h"
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *AParent, Qt::WindowFlags AFlags)
  : QMainWindow(AParent,AFlags)
{
  qDebug() << "MainWindow";
  FSystemIconset.openFile("system/default.jisp");
  connect(&FSystemIconset,SIGNAL(reseted(const QString &)),SLOT(onSkinChanged(const QString &)));
  FPluginManager = NULL;
  FSettings = NULL;
  createLayouts();
  createToolBars();
  createMenus();
  createActions();
  updateIcons();
}

MainWindow::~MainWindow()
{
  qDebug() << "~MainWindow";
}

bool MainWindow::init(IPluginManager *APluginManager, ISettings *ASettings)
{
  FPluginManager = APluginManager;
  FSettings = ASettings;
  if (FSettings)
  {
    connect(FSettings->instance(),SIGNAL(opened()),SLOT(onSettingsOpened()));
    connect(FSettings->instance(),SIGNAL(closed()),SLOT(onSettingsClosed()));
  }
  connectActions();
  return true;
}

bool MainWindow::start()
{
  show();
  return true;
}

void MainWindow::createLayouts()
{
  FUpperWidget = new QStackedWidget; 
  FUpperWidget->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
  FUpperWidget->setVisible(false);

  FRostersWidget = new QStackedWidget; 
  FRostersWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

  FBottomWidget = new QStackedWidget; 
  FBottomWidget->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);  
  FBottomWidget->setVisible(false);
                     
  FMainLayout = new QVBoxLayout;
  FMainLayout->addWidget(FUpperWidget);  
  FMainLayout->addWidget(FRostersWidget);  
  FMainLayout->addWidget(FBottomWidget);  

  QWidget *centralWidget = new QWidget;
  centralWidget->setLayout(FMainLayout); 
  setCentralWidget(centralWidget);
}

void MainWindow::createToolBars()
{
  FTopToolBar = addToolBar(tr("Top ToolBar"));
  FTopToolBar->setMovable(false); 
  addToolBar(Qt::TopToolBarArea,FTopToolBar);

  FBottomToolBar = addToolBar(tr("Bottom ToolBar"));
  FBottomToolBar->setMovable(false); 
  addToolBar(Qt::BottomToolBarArea,FBottomToolBar);
}

void MainWindow::createMenus()
{
  mnuMain = new Menu(0,"mainwindow::menu::mainmenu",this);
  mnuMain->setTitle(tr("Menu"));
  FBottomToolBar->addAction(mnuMain->menuAction()); 
}

void MainWindow::createActions()
{
  actQuit = new Action(1000,"mainwindow::action::quit",this);
  actQuit->setText(tr("Quit"));
  mnuMain->addAction(actQuit);
}

void MainWindow::connectActions()
{
  connect(actQuit,SIGNAL(triggered()),FPluginManager->instance(),SLOT(quit())); 
}

void MainWindow::onSettingsOpened()
{
  setGeometry(FSettings->value("window:geometry",geometry()).toRect());
}

void MainWindow::onSettingsClosed()
{
  if (isVisible())
    FSettings->setValue("window:geometry",geometry());
}

void MainWindow::onSkinChanged(const QString &/*ASkinName*/)
{
  updateIcons();
}

void MainWindow::closeEvent( QCloseEvent *AEvent )
{
  showMinimized();
  AEvent->ignore();
}

void MainWindow::updateIcons()
{
  if (mnuMain)
  {
    mnuMain->setIcon(FSystemIconset.iconByName("psi/jabber"));
    mnuMain->menuAction()->setIcon(mnuMain->icon());
  }

  if (actQuit)
    actQuit->setIcon(FSystemIconset.iconByName("psi/quit"));
}
