﻿/**
    OPA Editor: OPA operator widget
    OPA operator widget

    The MIT License (MIT)

    Source code copyright (c) 2013-2016 Frédéric Meslin
    Email: fredericmeslin@hotmail.com
    Website: www.fredslab.net
    Twitter: @marzacdev

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

*/

#include "programwidget.h"
#include "ui_programwidget.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QFileDialog>

#include "globals.h"
#include "programfile.h"
#include "mainwindow.h"

/*****************************************************************************/
ProgramWidget::ProgramWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProgramWidget)
{
    memset(&intProgramBuffer, 0, sizeof(OpaProgram));
    waitforIntProgram = false;
    programIndex = 0;
    ui->setupUi(this);
}

ProgramWidget::~ProgramWidget()
{
    delete ui;
}

/*****************************************************************************/
void ProgramWidget::updateUI()
{
    if (waitforIntProgram && !opa.isWaiting()) {
        QString name;
        programNameToQS(intProgramBuffer.params.name, name);
        ui->nameLine->setText(name);
        waitforIntProgram = false;
    }
}

/*****************************************************************************/
void ProgramWidget::setContent(const OpaProgramParams * params, bool send)
{
    opa.setEnable(false);

    setFlags(params->flags);

    QString name;
    programNameToQS(params->name, name);
    ui->nameLine->setText(name);
    ui->volumeDial->setValue(params->volume);
    ui->panningDial->setValue(params->panning);
    mainWindow->setAlgorithm(params->algorithm);

    opa.setEnable(true);

    if (send) {
        opa.programParamWrite(programIndex, OPA_CONFIG_ALGO, params->algorithm);
        opa.programParamWrite(programIndex, OPA_CONFIG_VOLUME, params->volume);
        opa.programParamWrite(programIndex, OPA_CONFIG_PANNING, params->panning);
        for(int i = 0; i < OPA_PROGS_NAME_LEN; i++)
            opa.programParamWrite(programIndex, OPA_CONFIG_NAME + i, params->name[i]);
        writeFlags();
   }
}

void ProgramWidget::getContent(OpaProgramParams * params)
{
    params->flags = getFlags();
    programNameFromQS(ui->nameLine->text(), params->name);
    params->volume = ui->volumeDial->value();
    params->panning = ui->panningDial->value();
    params->algorithm = currentAlgorithm;
}

/*****************************************************************************/
void ProgramWidget::setProgramIndex(int index)
{
    programIndex = index;
}

/*****************************************************************************/
void ProgramWidget::on_volumeDial_valueChanged(int value)
{
    opa.programParamWrite(programIndex, OPA_CONFIG_VOLUME, value);
}

void ProgramWidget::on_panningDial_valueChanged(int value)
{
    opa.programParamWrite(programIndex, OPA_CONFIG_PANNING, value);
}

/*****************************************************************************/
void ProgramWidget::on_initButton_clicked()
{
// Update the shield
    opa.programWrite(programIndex, &Opa::defaultProgram);

// Update the UI
    setContent(&Opa::defaultProgram.params, false);
    for (int o = 0; o < OPA_ALGOS_OP_NB; o++)
        editedOperators[o]->setContent(&Opa::defaultProgram.opParams[o], false);
}


/*****************************************************************************/
void ProgramWidget::on_openButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open OPA preset", QString(), "Presets Files (*.xml)");
    if (fileName.isEmpty()) return;

// Load the program
    OpaProgram prog;
    ProgramFile progFile(fileName);
    progFile.load(&prog);

// Update the shield
    opa.programWrite(programIndex, &prog);

// Update the UI
    setContent(&prog.params, false);
    for (int o = 0; o < OPA_ALGOS_OP_NB; o++)
        editedOperators[o]->setContent(&prog.opParams[o], false);
}

void ProgramWidget::on_saveButton_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save OPA preset", QString(), "Presets Files (*.xml)");
    if (fileName.isEmpty()) return;

// Read the interface
    OpaProgram prog;
    getContent(&prog.params);
    for (int o = 0; o < OPA_ALGOS_OP_NB; o++)
        editedOperators[o]->getContent(&prog.opParams[o]);

// Save the program
    ProgramFile progFile(fileName);
    progFile.save(&prog);
}

/*****************************************************************************/
void ProgramWidget::on_loadButton_clicked()
{
// Check the slot availability
    if (!opa.isConnected()) return;
    int slot = ui->slotSpin->value() - 1;
    if (slot < 0) return;

// Load the program
    opa.internalLoad(programIndex, slot);

// Update the UI
    mainWindow->programRead(programIndex);
}

void ProgramWidget::on_storeButton_clicked()
{
// Check memory protection
    if (!opa.isConnected()) return;
    if (mainWindow->ui->memoryProtectionAction->isChecked()) {
        QMessageBox mb(QMessageBox::Information, "OPA Editor", "Internal memory is protected!\n\nMemory protection can be disabled using the device menu.", QMessageBox::Ok, this);
        mb.exec();
        return;
    }

// Warn against data loss
    QMessageBox mb(QMessageBox::Warning, "OPA Editor", "This will discard previously stored program, continue?", QMessageBox::Yes | QMessageBox::No, this);
    if (mb.exec() == QMessageBox::No) return;

// Store the program
    int slot = ui->slotSpin->value() - 1;
    if (slot < 0) return;
    opa.internalStore(programIndex, slot);
}

/*****************************************************************************/
void ProgramWidget::on_nameLine_editingFinished()
{
    uint8_t name[OPA_PROGS_NAME_LEN];
    programNameFromQS(ui->nameLine->text(), name);
    for (int i = 0; i < OPA_PROGS_NAME_LEN; i++)
        opa.programParamWrite(programIndex, OPA_CONFIG_NAME + i, name[i]);
}

/*****************************************************************************/
void ProgramWidget::writeFlags()
{
    int flags = 0;
    flags |= ui->stealingButton->isChecked() ? OPA_PROGRAM_STEALING : 0;
    flags |= ui->muteButton->isChecked() ? OPA_PROGRAM_MUTED : 0;
    opa.programParamWrite(programIndex, OPA_CONFIG_FLAGS, flags);
}

int ProgramWidget::getFlags()
{
    int flags = 0;
    flags |= ui->stealingButton->isChecked() ? OPA_PROGRAM_STEALING : 0;
    flags |= ui->muteButton->isChecked() ? OPA_PROGRAM_MUTED : 0;
    return flags;
}

void ProgramWidget::setFlags(int flags)
{
    ui->stealingButton->setChecked(flags & OPA_PROGRAM_STEALING);
    ui->muteButton->setChecked(flags & OPA_PROGRAM_MUTED);
}

/*****************************************************************************/
void ProgramWidget::on_slotSpin_valueChanged(int value)
{
    opa.internalRead(value - 1, &intProgramBuffer);
    waitforIntProgram = true;
}

void ProgramWidget::on_stealingButton_clicked()
{
    writeFlags();
}

void ProgramWidget::on_muteButton_clicked()
{
    writeFlags();
}
