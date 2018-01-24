/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2011 ~ 2018 Deepin, Inc.
 *               2011 ~ 2018 Wang Yong
 *
 * Author:     Wang Yong <wangyong@deepin.com>
 * Maintainer: Wang Yong <wangyong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "window.h"
#include <DTitlebar>
#include <QLabel>
#include "dtoast.h"
#include <QDebug>
#include <QFileDialog>
#include <QScreen>
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QSvgWidget>
#include "dthememanager.h"
#include "utils.h"

Window::Window(DMainWindow *parent) : DMainWindow(parent)
{
    DThemeManager::instance()->setTheme("dark");

    installEventFilter(this);   // add event filter

    layoutWidget = new QWidget();
    layout = new QStackedLayout(layoutWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    this->setCentralWidget(layoutWidget);

    tabbar = new Tabbar(&editorMap);

    this->titlebar()->setCustomWidget(tabbar, Qt::AlignVCenter, false);
    this->titlebar()->setSeparatorVisible(true);
    this->titlebar()->setAutoHideOnFullscreen(true);

    connect(tabbar, &Tabbar::doubleClicked, this->titlebar(), &DTitlebar::doubleClicked, Qt::QueuedConnection);
    connect(tabbar, &Tabbar::switchToFile, this, &Window::handleSwitchToFile, Qt::QueuedConnection);
    connect(tabbar, &Tabbar::closeFile, this, &Window::handleCloseFile, Qt::QueuedConnection);
    connect(tabbar, &Tabbar::tabAddRequested, this, &Window::addBlankTab, Qt::QueuedConnection);
    connect(tabbar, &Tabbar::tabReleaseRequested, this, &Window::handleTabReleaseRequested, Qt::QueuedConnection);

    Utils::applyQss(this, "main.qss");
    Utils::applyQss(this->titlebar(), "main.qss");
}

Window::~Window()
{
    // We don't need clean pointers because application has exit here.
}

void Window::keyPressEvent(QKeyEvent *keyEvent)
{
    QString key = Utils::getKeymap(keyEvent);
    
    if (key == "Ctrl + T") {
        addBlankTab();
    } else if (key == "Ctrl + S") {
        saveFile();
    } else if (key == "Ctrl + Shift + S") {
        saveAsFile();
    } else if (key == "Ctrl + Tab") {
            tabbar->selectNextTab();
    } else if (key == "Ctrl + Shift + Backtab") {
        tabbar->selectPrevTab();
    } else if (key == "Ctrl + W") {
        tabbar->closeTab();
    } else if (key == "Ctrl + Shift + W") {
        tabbar->closeOtherTabs();
    } else if (key == "Ctrl + O") {
            openFile();
    } else if (key == "F11") {
        toggleFullscreen();
    }
}

int Window::isFileInTabs(QString file)
{
    return tabbar->isTabExist(file);
}

void Window::addTab(QString file)
{
    if (tabbar->isTabExist(file) == -1) {
        tabbar->addTab(file, QFileInfo(file).fileName());

        if (!editorMap.contains(file)) {
            Editor *editor = new Editor();
            editor->loadFile(file);

            editorMap[file] = editor;

            layout->addWidget(editor);
            layout->setCurrentWidget(editor);
        }
    }

    activateWindow();
}

void Window::addBlankTab()
{
    QString blankTabPath = QString("Blank Tab: %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")) ;

    tabbar->addTab(blankTabPath, "Blank document");
    Editor *editor = new Editor();
    editor->updatePath(blankTabPath);

    editorMap[blankTabPath] = editor;

    layout->addWidget(editor);
    layout->setCurrentWidget(editor);
}

void Window::handleSwitchToFile(QString filepath)
{
    if (editorMap.contains(filepath)) {
        layout->setCurrentWidget(editorMap[filepath]);
    }
}

void Window::handleCloseFile(QString filepath)
{
    if (editorMap.contains(filepath)) {
        Editor *editor = editorMap[filepath];

        layout->removeWidget(editor);
        editorMap.remove(filepath);

        editor->deleteLater();
    }

    // Exit window after close all tabs.
    if (editorMap.count() == 0) {
        deleteLater();
    }
}

void Window::activeTab(int index)
{
    activateWindow();
    tabbar->activeTab(index);
}

void Window::openFile()
{
    QFileDialog dialog(0, QString(), QDir::homePath());
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);

    if (dialog.exec()) {
        foreach (QString file, dialog.selectedFiles()) {
            addTab(file);
        }
    }
}

void Window::saveFile()
{
    if (tabbar->getActiveTabName() == "Blank document") {
        QString filepath = QFileDialog::getSaveFileName(this, "Save File", QDir::homePath());

        if (filepath != "") {
            QString tabPath = tabbar->getActiveTabPath();
            
            saveFileAsAnotherPath(tabPath, filepath);
        }
    } else {
        auto toast = new DToast(this);

        toast->setText("文件已自动保存");
        toast->setIcon(QIcon(Utils::getQrcPath("logo_24.svg")));
        toast->pop();

        toast->move((width() - toast->width()) / 2,
                    height() - toast->height() - notifyPadding);
    }
}

void Window::saveAsFile()
{
    QString filepath = QFileDialog::getSaveFileName(this, "Save File", QDir::homePath());
    QString tabPath = tabbar->getActiveTabPath();
    
    if (filepath != "" && filepath != tabPath) {
        saveFileAsAnotherPath(tabPath, filepath);
    }
}

void Window::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    }  else {
        showFullScreen();

        QScreen *screen = QGuiApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();

        auto toast = new DToast(this);

        toast->setText("按F11或Esc退出全屏");
        toast->setIcon(QIcon(Utils::getQrcPath("logo_24.svg")));
        toast->pop();

        toast->move((screenGeometry.width() - toast->width()) / 2, notifyPadding);
    }

}

Editor* Window::getActiveEditor()
{
    QString tabPath = tabbar->getActiveTabPath();

    return editorMap[tabPath];
}

void Window::saveFileAsAnotherPath(QString fromPath, QString toPath)
{
    tabbar->updateTab(tabbar->currentIndex(), toPath, QFileInfo(toPath).fileName());

    editorMap[toPath] = editorMap.take(fromPath);

    editorMap[toPath]->updatePath(toPath);
    editorMap[toPath]->saveFile();
}

void Window::handleTabReleaseRequested(QString tabName, QString filepath, int index)
{
    tabbar->closeTabWithIndex(index);
    
    QString content = editorMap[filepath]->textEditor->toPlainText();
    popTab(tabName, filepath, content);
}

void Window::addTabWithContent(QString tabName, QString filepath, QString content)
{
    tabbar->addTab(filepath, tabName);
    
    Editor *editor = new Editor();
    editor->updatePath(filepath);
    editor->textEditor->setPlainText(content);
    
    editorMap[filepath] = editor;

    layout->addWidget(editor);
    layout->setCurrentWidget(editor);
}
