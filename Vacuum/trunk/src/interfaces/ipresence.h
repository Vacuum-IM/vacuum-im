#ifndef IPRESENCE_H
#define IPRESENCE_H

#include <QList>
#include "../../interfaces/ixmppstreams.h"
#include "../../utils/jid.h"

#define PRESENCE_UUID "{511A07C4-FFA4-43ce-93B0-8C50409AFC0E}"

class IPresenceItem;

class IPresence
{
public:
  enum Show {
    Offline,
    Online,
    Chat,
    Away,
    DoNotDistrib,
    ExtendedAway,
    Invisible,
    Error
  };
public:
  virtual QObject *instance() =0;
  virtual const Jid &streamJid() const =0;
  virtual IXmppStream *xmppStream() const =0;
  virtual bool isOpen() const =0;
  virtual Show show() const =0;
  virtual bool setShow(Show AShow, const Jid &AToJid = Jid()) =0;
  virtual const QString &status() const =0;
  virtual bool setStatus(const QString &AStatus, const Jid &AToJid = Jid()) =0;
  virtual qint8 priority() const =0;
  virtual bool setPriority(qint8 APriority, const Jid &AToJid = Jid()) =0;
  virtual bool setPresence(Show AShow, const QString &AStatus, qint8 APriority, const Jid &AToJid = Jid()) =0;
  virtual IPresenceItem *item(const Jid &) const =0;
  virtual QList<IPresenceItem *> items() const =0;
  virtual QList<IPresenceItem *> items(const Jid &) const =0;
signals:
  virtual void opened() =0;
  virtual void selfPresence(int AShow, const QString &AStatus, qint8 APriority, const Jid &AToJid) =0;
  virtual void presenceItem(IPresenceItem *APresenceItem) =0;
  virtual void aboutToClose(int AShow, const QString &AStatus) =0;
  virtual void closed() =0;
};

class IPresenceItem
{
public:
  virtual QObject *instance() =0;
  virtual IPresence *presence() const =0;
  virtual const Jid &jid() const =0;
  virtual IPresence::Show show() const =0;
  virtual const QString &status() const =0;
  virtual qint8 priority() const =0;
};

class IPresencePlugin
{
public:
  virtual QObject *instance() =0;
  virtual IPresence *addPresence(IXmppStream *AXmppStream) =0;
  virtual IPresence *getPresence(const Jid &AStreamJid) const =0;
  virtual bool isContactOnline(const Jid &AContactJid) const =0;
  virtual QList<Jid> contactsOnline() const =0;
  virtual QList<IPresence *> contactPresences(const Jid &AContactJid) const =0;
  virtual void removePresence(IXmppStream *AXmppStream) =0;
signals:
  virtual void streamStateChanged(const Jid &AStreamJid, bool AStateOnline) =0;
  virtual void contactStateChanged(const Jid &AStreamJid, const Jid &AContactJid, bool AStateOnline) =0;
  virtual void presenceAdded(IPresence *APresence) =0;
  virtual void presenceOpened(IPresence *APresence) =0;
  virtual void selfPresence(IPresence *APresence, int AShow, const QString &AStatus, qint8 APriority, const Jid &AToJid) =0;
  virtual void presenceItem(IPresence *APresence, IPresenceItem *APresenceItem) =0;
  virtual void presenceAboutToClose(IPresence *APresence, int AShow, const QString &AStatus) =0;
  virtual void presenceClosed(IPresence *APresence) =0;
  virtual void presenceRemoved(IPresence *APresence) =0;
};

Q_DECLARE_INTERFACE(IPresenceItem,"Vacuum.Plugin.IPresenceItem/1.0")
Q_DECLARE_INTERFACE(IPresence,"Vacuum.Plugin.IPresence/1.0")
Q_DECLARE_INTERFACE(IPresencePlugin,"Vacuum.Plugin.IPresencePlugin/1.0")

#endif  //IPRESENCE_H