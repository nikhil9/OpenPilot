/**
 ******************************************************************************
 *
 * @file       pipxtremegadgetoptionspage.h
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @{
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef ESCGADGETOPTIONSPAGE_H
#define ESCGADGETOPTIONSPAGE_H

#include <qextserialport/src/qextserialenumerator.h>
#include "coreplugin/dialogs/ioptionspage.h"
#include "QString"
#include <QStringList>
#include <QDebug>

namespace Core {
	class IUAVGadgetConfiguration;
}

class EscGadgetConfiguration;

namespace Ui {
        class EscGadgetOptionsPage;
}

using namespace Core;

class EscGadgetOptionsPage : public IOptionsPage
{
Q_OBJECT
public:
    explicit EscGadgetOptionsPage(EscGadgetConfiguration *config, QObject *parent = 0);

    QWidget *createPage(QWidget *parent);
    void apply();
    void finish();

private:
        Ui::EscGadgetOptionsPage *options_page;
        EscGadgetConfiguration *m_config;
};

#endif
