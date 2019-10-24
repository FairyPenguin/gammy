/**
 * Copyright (C) 2019 Francesco Fusco. All rights reserved.
 * License: https://github.com/Fushko/gammy#license
 */

#include "ui_mainwindow.h"
#include "main.h"
#include "utils.h"
#include <math.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <QScreen>
#include <QSystemTrayIcon>
#include <QToolTip>
#include <QHelpEvent>
#include <QAction>
#include <QMenu>

#include <iostream>
#include <condition_variable>

#include "mainwindow.h"

#ifndef _WIN32
MainWindow::MainWindow(X11* x11, std::condition_variable* p) : ui(new Ui::MainWindow), trayIcon(new QSystemTrayIcon(this))
{
    this->pausethr = p;
    this->x11 = x11;

    init();
}
#endif

MainWindow::MainWindow(QWidget* parent, std::condition_variable* p) : QMainWindow(parent), ui(new Ui::MainWindow), trayIcon(new QSystemTrayIcon(this))
{
    this->pausethr = p;

    init();
}

void MainWindow::init()
{
    ui->setupUi(this);
    readConfig();

    auto appIcon = QIcon(":res/icons/16x16ball.ico");

    /*Set window properties */
    {
        this->setWindowTitle("Gammy");
        this->setWindowIcon(appIcon);

        resize(335, 333);

    #ifdef _WIN32
       this->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
       ui->extendBr->hide();
    #else
        this->setWindowFlags(Qt::Dialog);
        ui->closeButton->hide();
        ui->hideButton->hide();
    #endif

        ui->manBrSlider->hide();
    }

    /*Move window to bottom right */
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect screenGeometry = screen->availableGeometry();
        int h = screenGeometry.height();
        int w = screenGeometry.width();
        this->move(w - this->width(), h - this->height());
    }

    /*Create tray icon */
    {
        if (!QSystemTrayIcon::isSystemTrayAvailable())
        {
#ifdef dbg
            std::cout << "Qt: System tray unavailable.\n";
#endif
            ignore_closeEvent = false;
            MainWindow::show();
        }

        this->trayIcon->setIcon(appIcon);

        auto menu = this->createMenu();
        this->trayIcon->setContextMenu(menu);
        this->trayIcon->setToolTip(QString("Gammy"));
        this->trayIcon->show();
        connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::iconActivated);
    }

    /*Set slider properties*/
    {
        ui->extendBr->setChecked(cfg[toggleLimit]);
        setBrSlidersRange(cfg[toggleLimit]);

        ui->tempSlider->setRange(0, temp_arr_entries * temp_mult);
        ui->minBrSlider->setValue(cfg[MinBr]);
        ui->maxBrSlider->setValue(cfg[MaxBr]);
        ui->offsetSlider->setValue(cfg[Offset]);
        ui->speedSlider->setValue(cfg[Speed]);
        ui->tempSlider->setValue(cfg[Temp]);
        ui->thresholdSlider->setValue(cfg[Threshold]);
        ui->pollingSlider->setValue(cfg[Polling_rate]);
    }

    /*Set auto check */
    {
        ui->autoCheck->setChecked(cfg[isAuto]);

        run = cfg[isAuto];
        pausethr->notify_one();

        toggleSliders(cfg[isAuto]);
    }
}

void MainWindow::updatePollingSlider(int min, int max)
{
   const auto poll = cfg[Polling_rate];

   ui->pollingSlider->setRange(min, max);

   if(poll < min)
   {
       cfg[Polling_rate] = min;
   }
   else if(poll > max)
   {
       cfg[Polling_rate] = max;
   }

   ui->pollingLabel->setText(QString::number(poll));
   ui->pollingSlider->setValue(poll);
}

QMenu* MainWindow::createMenu()
{
    auto menu = new QMenu(this);

#ifdef _WIN32
    QAction* startupAction = new QAction("&Run at startup", this);
    startupAction->setCheckable(true);
    connect(startupAction, &QAction::triggered, [=]{toggleRegkey(startupAction->isChecked());});
    menu->addAction(startupAction);

    LRESULT s = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"Gammy", RRF_RT_REG_SZ, nullptr, nullptr, nullptr);

    if (s == ERROR_SUCCESS)
    {
        startupAction->setChecked(true);
    }
    else startupAction->setChecked(false);
#else
    QAction* showAction = new QAction("&Open Gammy", this);
    connect(showAction, &QAction::triggered, this, [=]{updateBrLabel(); MainWindow::show();});
    menu->addAction(showAction);
#endif

    QAction* quitPrevious = new QAction("&Quit", this);
    connect(quitPrevious, &QAction::triggered, this, [=]{on_closeButton_clicked();});
    menu->addAction(quitPrevious);

#ifndef _WIN32
    QAction* quitPure = new QAction("&Quit (set pure gamma)", this);
    connect(quitPure, &QAction::triggered, this, [=]{set_previous_gamma = false; on_closeButton_clicked(); });
    menu->addAction(quitPure);
#endif

    return menu;
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger)
    {
        MainWindow::updateBrLabel();
        MainWindow::show();
    }
}

void MainWindow::updateBrLabel()
{
    int val = scr_br * 100 / 255;
    ui->statusLabel->setText(QStringLiteral("%1").arg(val));
}

void MainWindow::on_hideButton_clicked()
{
    this->hide();
}

void MainWindow::on_closeButton_clicked()
{
    run = true;
    quit = true;
    pausethr->notify_one();

    QCloseEvent e;
    e.setAccepted(true);
    emit closeEvent(&e);

    trayIcon->hide();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    MainWindow::hide();
    saveConfig();

    if(ignore_closeEvent) e->ignore();
}

//___________________________________________________________

void MainWindow::on_minBrSlider_valueChanged(int val)
{   
    ui->minBrLabel->setText(QStringLiteral("%1").arg(val * 100 / 255));

    if(val > cfg[MaxBr])
    {
        ui->maxBrSlider->setValue(cfg[MaxBr] = val);
    }

    cfg[MinBr] = val;
}

void MainWindow::on_maxBrSlider_valueChanged(int val)
{
    ui->maxBrLabel->setText(QStringLiteral("%1").arg(val * 100 / 255));

    if(val < cfg[MinBr])
    {
        ui->minBrSlider->setValue(cfg[MinBr] = val);
    }

    cfg[MaxBr] = val;
}

void MainWindow::on_offsetSlider_valueChanged(int val)
{
    cfg[Offset] = val;

    ui->offsetLabel->setText(QStringLiteral("%1").arg(val * 100 / 255));
}

void MainWindow::on_speedSlider_valueChanged(int val)
{
    cfg[Speed] = val;
}

void MainWindow::on_tempSlider_valueChanged(int val)
{
    cfg[Temp] = val;

#ifdef _WIN32
    setGDIGamma(scr_br, val);
#else
    x11->setXF86Gamma(scr_br, val);
#endif

    int temp_kelvin = convertToRange(temp_arr_entries * temp_mult - val, 0, temp_arr_entries * temp_mult, min_temp_kelvin, max_temp_kelvin);
    temp_kelvin = ((temp_kelvin - 1) / 100 + 1) * 100;

    ui->tempLabel->setText(QStringLiteral("%1").arg(temp_kelvin));
}

void MainWindow::on_thresholdSlider_valueChanged(int val)
{
    cfg[Threshold] = val;
}

void MainWindow::on_pollingSlider_valueChanged(int val)
{
    cfg[Polling_rate] = val;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_autoCheck_stateChanged(int state)
{
    if(state == 2)
    {
        cfg[isAuto] = 1;
        run = true;
        if(force) *force = true;
    }
    else
    {
        cfg[isAuto] = 0;
        run = false;
    }

    toggleSliders(run);
    pausethr->notify_one();
}

void MainWindow::toggleSliders(bool is_auto)
{
    if(is_auto)
    {
        ui->manBrSlider->hide();
    }
    else
    {
        ui->manBrSlider->setValue(scr_br);
        ui->manBrSlider->show();
    }
}

void MainWindow::on_manBrSlider_valueChanged(int value)
{
    scr_br = value;
#ifdef _WIN32
    setGDIBrightness(scr_br, cfg[Temp]);
#else
    x11->setXF86Gamma(scr_br, cfg[Temp]);
#endif

    MainWindow::updateBrLabel();
}

void MainWindow::on_extendBr_clicked(bool checked)
{
    cfg[toggleLimit] = checked;

    setBrSlidersRange(cfg[toggleLimit]);
}

void MainWindow::setBrSlidersRange(bool inc)
{
    int br_limit = default_brightness;

    if(inc) br_limit *= 2;

    ui->manBrSlider->setRange(64, br_limit);
    ui->minBrSlider->setRange(64, br_limit);
    ui->maxBrSlider->setRange(64, br_limit);
    ui->offsetSlider->setRange(0, br_limit);
}

#ifdef _WIN32
void MainWindow::mousePressEvent(QMouseEvent* e)
{
   mouse = e->pos();

   windowPressed = true;
}

void MainWindow::mouseReleaseEvent(QMouseEvent* e)
{
   windowPressed = false;
}

void MainWindow::mouseMoveEvent(QMouseEvent* e)
{
   if (windowPressed)
   {
       move(e->globalX() - mouse.x(), e->globalY() - mouse.y());
   }
}
#endif
