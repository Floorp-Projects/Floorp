#ifndef MOZQWIDGET_H
#define MOZQWIDGET_H

#include <qwidget.h>

class QEvent;
class nsCommonWidget;

class MozQWidget : public QWidget
{
    Q_OBJECT
public:
    MozQWidget(nsCommonWidget *receiver, QWidget *parent,
               const char *name, WFlags f);

    /**
     * Mozilla helper.
     */
    void setModal(bool);
protected:
    virtual bool event(QEvent *ev);
private:
    nsCommonWidget *mReceiver;
};

#endif
