#include "filestreamsmanager.h"

#include <QSet>
#include <QDir>
#include <QComboBox>
#include <QStandardPaths>
#include <definitions/namespaces.h>
#include <definitions/menuicons.h>
#include <definitions/resources.h>
#include <definitions/shortcuts.h>
#include <definitions/actiongroups.h>
#include <definitions/optionvalues.h>
#include <definitions/optionnodes.h>
#include <definitions/optionnodeorders.h>
#include <definitions/optionwidgetorders.h>
#include <definitions/internalerrors.h>
#include <utils/widgetmanager.h>
#include <utils/iconstorage.h>
#include <utils/shortcuts.h>
#include <utils/datetime.h>
#include <utils/options.h>
#include <utils/stanza.h>
#include <utils/logger.h>
#include <utils/jid.h>

FileStreamsManager::FileStreamsManager()
{
	FDataManager = NULL;
	FOptionsManager = NULL;
	FTrayManager = NULL;
	FMainWindowPlugin = NULL;
}

FileStreamsManager::~FileStreamsManager()
{

}

void FileStreamsManager::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("File Streams Manager");
	APluginInfo->description = tr("Allows to initiate a thread for transferring files between two XMPP entities");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Potapov S.A. aka Lion";
	APluginInfo->homePage = "http://www.vacuum-im.org";
	APluginInfo->dependences.append(DATASTREAMSMANAGER_UUID);
}

bool FileStreamsManager::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);
	IPlugin *plugin = APluginManager->pluginInterface("IDataStreamsManager").value(0,NULL);
	if (plugin)
	{
		FDataManager = qobject_cast<IDataStreamsManager *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IMainWindowPlugin").value(0,NULL);
	if (plugin)
	{
		FMainWindowPlugin = qobject_cast<IMainWindowPlugin *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("ITrayManager").value(0,NULL);
	if (plugin)
	{
		FTrayManager = qobject_cast<ITrayManager *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IOptionsManager").value(0,NULL);
	if (plugin)
	{
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());
		if (FOptionsManager)
		{
			connect(FOptionsManager->instance(),SIGNAL(profileClosed(const QString &)),SLOT(onProfileClosed(const QString &)));
		}
	}

	return FDataManager!=NULL;
}

bool FileStreamsManager::initObjects()
{
	Shortcuts::declareShortcut(SCT_APP_FILETRANSFERS_SHOW,tr("Show file transfers"),tr("Ctrl+T","Show file transfers"),Shortcuts::ApplicationShortcut);

	XmppError::registerError(NS_INTERNAL_ERROR,IERR_FILESTREAMS_STREAM_FILE_IO_ERROR,tr("File input/output error"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_FILESTREAMS_STREAM_FILE_SIZE_CHANGED,tr("File size unexpectedly changed"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_FILESTREAMS_STREAM_CONNECTION_TIMEOUT,tr("Connection timed out"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_FILESTREAMS_STREAM_TERMINATED_BY_REMOTE_USER,tr("Data transmission terminated by remote user"));

	if (FDataManager)
	{
		FDataManager->insertProfile(this);
	}

	if (FTrayManager || FMainWindowPlugin)
	{
		Action *action = new Action(this);
		action->setText(tr("File Transfers"));
		action->setIcon(RSR_STORAGE_MENUICONS,MNI_FILESTREAMSMANAGER);
		action->setShortcutId(SCT_APP_FILETRANSFERS_SHOW);
		connect(action,SIGNAL(triggered(bool)),SLOT(onShowFileStreamsWindow(bool)));
	
		if (FMainWindowPlugin)
			FMainWindowPlugin->mainWindow()->mainMenu()->addAction(action,AG_MMENU_FILESTREAMS_TRANSFER,true);
		
		if (FTrayManager)
			FTrayManager->contextMenu()->addAction(action, AG_TMTM_FILESTREAMS_TRANSFER, true);
	}
	
	return true;
}

bool FileStreamsManager::initSettings()
{
	QStringList availMethods = FDataManager!=NULL ? FDataManager->methods() : QStringList();
	Options::setDefaultValue(OPV_FILESTREAMS_DEFAULTDIR,QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
	Options::setDefaultValue(OPV_FILESTREAMS_GROUPBYSENDER,false);
	Options::setDefaultValue(OPV_FILESTREAMS_DEFAULTMETHOD,NS_SOCKS5_BYTESTREAMS);
	Options::setDefaultValue(OPV_FILESTREAMS_ACCEPTABLEMETHODS,availMethods);

	if (FOptionsManager)
	{
		FOptionsManager->insertOptionsDialogHolder(this);
	}

	return true;
}

QMultiMap<int, IOptionsDialogWidget *> FileStreamsManager::optionsDialogWidgets(const QString &ANodeId, QWidget *AParent)
{
	QMultiMap<int, IOptionsDialogWidget *> widgets;
	if (FOptionsManager && ANodeId==OPN_DATATRANSFER)
	{
		widgets.insertMulti(OHO_DATATRANSFER_FILETRANSFER, FOptionsManager->newOptionsDialogHeader(tr("File transfer"),AParent));
		widgets.insertMulti(OWO_DATATRANSFER_FILESTREAMS, new FileStreamsOptionsWidget(this,AParent));
		widgets.insertMulti(OWO_DATATRANSFER_GROUPBYSENDER,FOptionsManager->newOptionsDialogWidget(Options::node(OPV_FILESTREAMS_GROUPBYSENDER),tr("Create separate folder for each sender"),AParent));

		if (FDataManager)
		{
			QComboBox *cmbDefaultMethod = new QComboBox(AParent);
			foreach(const QString &methodId, FDataManager->methods())
			{
				IDataStreamMethod *method = FDataManager->method(methodId);
				cmbDefaultMethod->addItem(method->methodName(), method->methodNS());
			}
			widgets.insertMulti(OWO_DATATRANSFER_DEFAULTMETHOD,FOptionsManager->newOptionsDialogWidget(Options::node(OPV_FILESTREAMS_DEFAULTMETHOD),tr("Default transfer method:"),cmbDefaultMethod,AParent));
		}
	}
	return widgets;
}

QString FileStreamsManager::dataStreamProfile() const
{
	return NS_SI_FILETRANSFER;
}

bool FileStreamsManager::dataStreamMakeRequest(const QString &AStreamId, Stanza &ARequest) const
{
	IFileStream *stream = findStream(AStreamId);
	if (stream!=NULL && stream->streamKind()==IFileStream::SendFile)
	{
		if (!stream->fileName().isEmpty() && stream->fileSize()>0)
		{
			QDomElement siElem = ARequest.firstElement("si",NS_STREAM_INITIATION);
			if (!siElem.isNull())
			{
				siElem.setAttribute("mime-type","text/plain");
				QDomElement fileElem = siElem.appendChild(ARequest.createElement("file",NS_SI_FILETRANSFER)).toElement();
				fileElem.setAttribute("name",stream->fileName().split("/").last());
				fileElem.setAttribute("size",stream->fileSize());
				if (!stream->fileHash().isEmpty())
					fileElem.setAttribute("hash",stream->fileHash());
				if (!stream->fileDate().isValid())
					fileElem.setAttribute("date",DateTime(stream->fileDate()).toX85Date());
				if (!stream->fileDescription().isEmpty())
					fileElem.appendChild(ARequest.createElement("desc")).appendChild(ARequest.createTextNode(stream->fileDescription()));
				if (stream->isRangeSupported())
					fileElem.appendChild(ARequest.createElement("range"));
				return true;
			}
			else
			{
				LOG_STRM_ERROR(stream->streamJid(),QString("Failed to insert data stream request, sid=%1: SI element not found").arg(AStreamId));
			}
		}
		else
		{
			LOG_STRM_ERROR(stream->streamJid(),QString("Failed to insert data stream request, sid=%1: File not found or empty").arg(AStreamId));
		}
	}
	else if (stream)
	{
		LOG_STRM_ERROR(stream->streamJid(),QString("Failed to insert data stream request, sid=%1: Invalid stream kind").arg(AStreamId));
	}
	else
	{
		LOG_ERROR(QString("Failed to insert data stream request, sid=%1: Stream not found").arg(AStreamId));
	}
	return false;
}

bool FileStreamsManager::dataStreamMakeResponse(const QString &AStreamId, Stanza &AResponse) const
{
	IFileStream *stream = findStream(AStreamId);
	if (stream!=NULL && stream->streamKind()==IFileStream::ReceiveFile)
	{
		if (stream->isRangeSupported() && (stream->rangeOffset()>0 || stream->rangeLength()>0))
		{
			QDomElement siElem = AResponse.firstElement("si",NS_STREAM_INITIATION);
			if (!siElem.isNull())
			{
				QDomElement fileElem = siElem.appendChild(AResponse.createElement("file",NS_SI_FILETRANSFER)).toElement();
				QDomElement rangeElem = fileElem.appendChild(AResponse.createElement("range")).toElement();
				if (stream->rangeOffset()>0)
					rangeElem.setAttribute("offset",stream->rangeOffset());
				if (stream->rangeLength()>0)
					rangeElem.setAttribute("length",stream->rangeLength());
			}
			else
			{
				LOG_STRM_ERROR(stream->streamJid(),QString("Failed to set range in data stream response, sid=%1: SI element not found").arg(AStreamId));
			}
		}
		return true;
	}
	else if (stream)
	{
		LOG_STRM_ERROR(stream->streamJid(),QString("Failed to insert data stream response, sid=%1: Invalid stream kind").arg(AStreamId));
	}
	else
	{
		LOG_ERROR(QString("Failed to insert data stream response, sid=%1: Stream not found").arg(AStreamId));
	}
	return false;
}

bool FileStreamsManager::dataStreamProcessRequest(const QString &AStreamId, const Stanza &ARequest, const QList<QString> &AMethods)
{
	if (!AMethods.isEmpty() && !FStreams.contains(AStreamId))
	{
		for (QMultiMap<int, IFileStreamHandler *>::const_iterator it = FHandlers.constBegin(); it!=FHandlers.constEnd(); ++it)
		{
			IFileStreamHandler *handler = it.value();
			if (handler->fileStreamProcessRequest(it.key(),AStreamId,ARequest,AMethods))
				return true;
		}
		LOG_STRM_WARNING(ARequest.to(),QString("Failed to process file stream request, sid=%1: Stream handler not found").arg(AStreamId));
	}
	else if (AMethods.isEmpty())
	{
		LOG_STRM_ERROR(ARequest.to(),QString("Failed to process file stream request, sid=%1: No valid stream methods").arg(AStreamId));
	}
	else
	{
		LOG_STRM_ERROR(ARequest.to(),QString("Failed to process file stream request, sid=%1: Duplicate stream id").arg(AStreamId));
	}
	return false;
}

bool FileStreamsManager::dataStreamProcessResponse(const QString &AStreamId, const Stanza &AResponse, const QString &AMethod)
{
	IFileStreamHandler *handler = findStreamHandler(AStreamId);
	if (handler != NULL)
		return handler->fileStreamProcessResponse(AStreamId,AResponse,AMethod);
	else
		LOG_STRM_ERROR(AResponse.to(),QString("Failed to process file stream response, sid=%1: Stream handler not found").arg(AStreamId));
	return false;
}

bool FileStreamsManager::dataStreamProcessError(const QString &AStreamId, const XmppError &AError)
{
	IFileStream *stream = findStream(AStreamId);
	if (stream != NULL)
	{
		stream->abortStream(AError);
		return true;
	}
	else
	{
		LOG_ERROR(QString("Failed to process file stream error, sid=%1: Stream not found").arg(AStreamId));
	}
	return false;
}

QList<IFileStream *> FileStreamsManager::streams() const
{
	return FStreams.values();
}

IFileStream *FileStreamsManager::findStream(const QString &AStreamId) const
{
	return FStreams.value(AStreamId);
}

IFileStreamHandler *FileStreamsManager::findStreamHandler(const QString &AStreamId) const
{
	return FStreamHandler.value(AStreamId);
}

IFileStream *FileStreamsManager::createStream(IFileStreamHandler *AHandler, const QString &AStreamId, const Jid &AStreamJid, const Jid &AContactJid, IFileStream::StreamKind AKind, QObject *AParent)
{
	if (FDataManager && AHandler && !AStreamId.isEmpty() && !FStreams.contains(AStreamId))
	{
		LOG_STRM_INFO(AStreamJid,QString("Creating file stream, sid=%1, with=%2, kind=%3").arg(AStreamId,AContactJid.full()).arg(AKind));
		
		IFileStream *stream = new FileStream(FDataManager,AStreamId,AStreamJid,AContactJid,AKind,AParent);
		connect(stream->instance(),SIGNAL(streamDestroyed()),SLOT(onStreamDestroyed()));
		
		FStreams.insert(AStreamId,stream);
		FStreamHandler.insert(AStreamId,AHandler);
		
		emit streamCreated(stream);
		return stream;
	}
	else if (FDataManager && AHandler)
	{
		LOG_STRM_ERROR(AStreamJid,QString("Failed to create file stream, sid=%1: Invalid params").arg(AStreamId));
	}

	return NULL;
}

QList<IFileStreamHandler *> FileStreamsManager::streamHandlers() const
{
	return FHandlers.values();
}

void FileStreamsManager::insertStreamsHandler(int AOrder, IFileStreamHandler *AHandler)
{
	if (AHandler!=NULL && !FHandlers.contains(AOrder,AHandler))
	{
		FHandlers.insertMulti(AOrder,AHandler);
		emit streamHandlerInserted(AOrder, AHandler);
	}
}

void FileStreamsManager::removeStreamsHandler(int AOrder, IFileStreamHandler *AHandler)
{
	if (FHandlers.contains(AOrder,AHandler))
	{
		FHandlers.remove(AOrder,AHandler);
		emit streamHandlerRemoved(AOrder,AHandler);
	}
}

void FileStreamsManager::onStreamDestroyed()
{
	IFileStream *stream = qobject_cast<IFileStream *>(sender());
	if (stream != NULL)
	{
		LOG_STRM_INFO(stream->streamJid(),QString("File stream destroyed, sid=%1").arg(stream->streamId()));
		FStreams.remove(stream->streamId());
		FStreamHandler.remove(stream->streamId());
		emit streamDestroyed(stream);
	}
}

void FileStreamsManager::onShowFileStreamsWindow(bool)
{
	if (FFileStreamsWindow.isNull())
	{
		FFileStreamsWindow = new FileStreamsWindow(this, NULL);
		WidgetManager::setWindowSticky(FFileStreamsWindow,true);
	}
	WidgetManager::showActivateRaiseWindow(FFileStreamsWindow);
}

void FileStreamsManager::onProfileClosed(const QString &AName)
{
	Q_UNUSED(AName);

	if (!FFileStreamsWindow.isNull())
		delete FFileStreamsWindow;

	foreach(IFileStream *stream, FStreams.values())
		delete stream->instance();
}
