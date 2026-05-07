#ifndef TEST_H
#define TEST_H

#include <QObject>
#include "camworker.h"

class Test : public QObject
{
    Q_OBJECT
public:
    explicit Test(QObject* parent);
};

#endif // TEST_H
