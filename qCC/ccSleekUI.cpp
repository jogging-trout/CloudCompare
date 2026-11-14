// ##########################################################################
// #                                                                        #
// #                              CLOUDCOMPARE                              #
// #                                                                        #
// #  This program is free software; you can redistribute it and/or modify  #
// #  it under the terms of the GNU General Public License as published by  #
// #  the Free Software Foundation; version 2 or later of the License.      #
// #                                                                        #
// #  This program is distributed in the hope that it will be useful,       #
// #  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
// #  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
// #  GNU General Public License for more details.                          #
// #                                                                        #
// ##########################################################################

#include "ccSleekUI.h"

#include "ccContourLinesGenerator.h"
#include "ccDBRoot.h"
#include "ccGraphicalSegmentationTool.h"
#include "ccGraphicalTransformationTool.h"
#include "ccSleekUndo.h"
#include "mainwindow.h"
#include "pluginManager/ccPluginUIManager.h"

#include "ui_mainWindow.h"

// qCC_db
#include <ccGenericMesh.h>
#include <ccLog.h>
#include <ccMesh.h>
#include <ccPointCloud.h>
#include <ccPolyline.h>
#include <ccProgressDialog.h>
#include <ccRasterGrid.h>

// qCC_glWindow
#include <ccGLWindowInterface.h>
#include <ccGuiParameters.h>

// qCC_io
#include <FileIOFilter.h>

// CCCoreLib
#include <CloudSamplingTools.h>

// Qt
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStatusBar>
#include <QStyleFactory>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoView>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <cmath>

namespace
{
	// persistent settings
	const char* S_GROUP          = "SleekUI";
	const char* S_THEME          = "Theme";
	const char* S_LAYOUT_VERSION = "LayoutVersion";
	const int   LAYOUT_VERSION   = 1;

	//! Colors of a theme
	struct Palette
	{
		const char* window;    // window and bars
		const char* panel;     // panels, lists, fields
		const char* raised;    // buttons
		const char* border;    // hairlines
		const char* text;      // main text
		const char* textSoft;  // secondary text and icons
		const char* textOff;   // disabled text and icons
		const char* hover;     // hovered item
		const char* accent;    // accent color
		const char* accentBg;  // selected item background
		const char* accentTxt; // selected item text
		const char* viewport;  // default 3D background
		const char* points;    // default points color
		const char* eye;       // suffix of the eye icons
	};

	const Palette& GetPalette(ccSleekUI::Theme theme)
	{
		static const Palette s_light{"#F3F4F6", "#FFFFFF", "#FFFFFF", "#D9DDE3", "#1B1F27", "#4A5361", "#A3AAB5", "#E6E9EE", "#2563EB", "#DBE6FD", "#123A8F", "#E9ECF0", "#2A2F38", "light"};
		static const Palette s_dark{"#1C1E22", "#24272C", "#2D3138", "#3A3F47", "#E8EAEE", "#B4BBC6", "#69707B", "#30353D", "#6EA8FF", "#2A4168", "#DCE9FF", "#15171A", "#E3E6EB", "dark"};
		return theme == ccSleekUI::DARK ? s_dark : s_light;
	}

	QString BuildStyleSheet(const Palette& p)
	{
		QString qss = QStringLiteral(R"(
QMainWindow, QDialog { background: %WINDOW%; }
QMainWindow::separator { background: %BORDER%; width: 1px; height: 1px; }
QToolTip { background: %PANEL%; color: %TEXT%; border: 1px solid %BORDER%; padding: 4px 6px; }

QMenuBar { background: %WINDOW%; color: %TEXT%; border-bottom: 1px solid %BORDER%; }
QMenuBar::item { background: transparent; padding: 5px 10px; border-radius: 5px; }
QMenuBar::item:selected { background: %HOVER%; }
QMenuBar::item:disabled { color: %TEXTOFF%; }
QMenu { background: %PANEL%; color: %TEXT%; border: 1px solid %BORDER%; padding: 5px; }
QMenu::item { padding: 6px 26px 6px 30px; border-radius: 5px; }
QMenu::icon { padding-left: 8px; }
QMenu::item:selected { background: %ACCENTBG%; color: %ACCENTTXT%; }
QMenu::item:disabled { color: %TEXTOFF%; }
QMenu::separator { height: 1px; background: %BORDER%; margin: 5px 6px; }

QToolBar { background: %WINDOW%; border: none; spacing: 2px; padding: 4px; }
QToolBar#sleekTopBar { border-bottom: 1px solid %BORDER%; }
QToolBar#sleekRail { border-right: 1px solid %BORDER%; padding: 6px 4px; spacing: 3px; }
QToolBar::separator { background: %BORDER%; width: 1px; height: 1px; margin: 5px; }
QToolButton { color: %TEXTSOFT%; background: transparent; border: 1px solid transparent; border-radius: 7px; padding: 5px; }
QToolButton:hover { background: %HOVER%; color: %TEXT%; }
QToolButton:pressed { background: %ACCENTBG%; }
QToolButton:checked { background: %ACCENTBG%; color: %ACCENTTXT%; border-color: %ACCENT%; }
QToolButton:disabled { color: %TEXTOFF%; }
QToolButton::menu-indicator { image: none; }
QToolBar#sleekRail QToolButton { min-width: 62px; padding: 6px 2px 4px 2px; font-size: 11px; }

QDockWidget { color: %TEXT%; font-weight: 600; }
QDockWidget::title { background: %WINDOW%; padding: 7px 10px; border-bottom: 1px solid %BORDER%; }
QTreeView, QListView, QTableView, QTextEdit, QPlainTextEdit, QListWidget {
	background: %PANEL%; color: %TEXT%; border: none; alternate-background-color: %PANEL%;
	selection-background-color: %ACCENTBG%; selection-color: %ACCENTTXT%; outline: 0; }
QTreeView::item, QListView::item { padding: 4px 2px; border-radius: 4px; }
QTreeView::item:hover, QListView::item:hover { background: %HOVER%; }
QTreeView::item:selected, QListView::item:selected { background: %ACCENTBG%; color: %ACCENTTXT%; }
QTreeView#dbTreeView::indicator { width: 18px; height: 18px; }
QTreeView#dbTreeView::indicator:checked, QTreeView#dbTreeView::indicator:indeterminate { image: url(:/sleek/eye-%EYE%.svg); }
QTreeView#dbTreeView::indicator:unchecked { image: url(:/sleek/eye-off-%EYE%.svg); }
QHeaderView::section { background: %WINDOW%; color: %TEXTSOFT%; border: none; border-bottom: 1px solid %BORDER%; padding: 5px 8px; }

QPushButton { background: %RAISED%; color: %TEXT%; border: 1px solid %BORDER%; border-radius: 6px; padding: 5px 12px; }
QPushButton:hover { background: %HOVER%; }
QPushButton:pressed, QPushButton:checked { background: %ACCENTBG%; color: %ACCENTTXT%; border-color: %ACCENT%; }
QPushButton:disabled { color: %TEXTOFF%; }
QPushButton#sleekPrimary { background: %ACCENT%; color: %ONACCENT%; border-color: %ACCENT%; font-weight: 600; }
QPushButton#sleekPrimary:hover { border-color: %TEXT%; }

QLabel { color: %TEXT%; }
QLabel#sleekHint { color: %TEXTSOFT%; }
QLabel#sleekPanelTitle { font-weight: 600; font-size: 13px; }
QGroupBox { color: %TEXT%; border: 1px solid %BORDER%; border-radius: 6px; margin-top: 10px; padding-top: 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }

QStatusBar { background: %WINDOW%; color: %TEXTSOFT%; border-top: 1px solid %BORDER%; }
QStatusBar::item { border: none; }
QMdiArea { background: %WINDOW%; }
QTabBar::tab { background: %WINDOW%; color: %TEXTSOFT%; padding: 6px 12px; border: none; }
QTabBar::tab:selected { color: %TEXT%; border-bottom: 2px solid %ACCENT%; }

QScrollBar:vertical { background: transparent; width: 11px; margin: 0; }
QScrollBar:horizontal { background: transparent; height: 11px; margin: 0; }
QScrollBar::handle { background: %BORDER%; border-radius: 4px; margin: 2px; }
QScrollBar::handle:vertical { min-height: 28px; }
QScrollBar::handle:horizontal { min-width: 28px; }
QScrollBar::handle:hover { background: %TEXTOFF%; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
)");
		qss.replace("%WINDOW%", p.window);
		qss.replace("%PANEL%", p.panel);
		qss.replace("%RAISED%", p.raised);
		qss.replace("%BORDER%", p.border);
		qss.replace("%TEXTSOFT%", p.textSoft);
		qss.replace("%TEXTOFF%", p.textOff);
		qss.replace("%TEXT%", p.text);
		qss.replace("%HOVER%", p.hover);
		qss.replace("%ACCENTBG%", p.accentBg);
		qss.replace("%ACCENTTXT%", p.accentTxt);
		qss.replace("%ONACCENT%", QString(p.eye) == "dark" ? "#0E1A30" : "#FFFFFF");
		qss.replace("%ACCENT%", p.accent);
		qss.replace("%EYE%", p.eye);
		return qss;
	}

	QPalette BuildQPalette(const Palette& p)
	{
		QPalette pal;
		pal.setColor(QPalette::Window, QColor(p.window));
		pal.setColor(QPalette::WindowText, QColor(p.text));
		pal.setColor(QPalette::Base, QColor(p.panel));
		pal.setColor(QPalette::AlternateBase, QColor(p.window));
		pal.setColor(QPalette::Text, QColor(p.text));
		pal.setColor(QPalette::Button, QColor(p.raised));
		pal.setColor(QPalette::ButtonText, QColor(p.text));
		pal.setColor(QPalette::BrightText, QColor(p.text));
		pal.setColor(QPalette::ToolTipBase, QColor(p.panel));
		pal.setColor(QPalette::ToolTipText, QColor(p.text));
		pal.setColor(QPalette::PlaceholderText, QColor(p.textOff));
		pal.setColor(QPalette::Highlight, QColor(p.accent));
		pal.setColor(QPalette::HighlightedText, QColor(QString(p.eye) == "dark" ? "#0E1A30" : "#FFFFFF"));
		pal.setColor(QPalette::Link, QColor(p.accent));
		pal.setColor(QPalette::Light, QColor(p.hover));
		pal.setColor(QPalette::Midlight, QColor(p.hover));
		pal.setColor(QPalette::Mid, QColor(p.border));
		pal.setColor(QPalette::Dark, QColor(p.border));
		pal.setColor(QPalette::Shadow, QColor(p.border));
		for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
			pal.setColor(QPalette::Disabled, role, QColor(p.textOff));
		return pal;
	}

	QPixmap RenderIcon(const QByteArray& svg, const QColor& color)
	{
		QByteArray data = svg;
		data.replace("#STROKE", color.name().toLatin1());
		QSvgRenderer renderer(data);
		QPixmap      pixmap(72, 72);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		renderer.render(&painter);
		return pixmap;
	}

	QIcon MakeIcon(const QString& name, const Palette& p)
	{
		QFile file(QStringLiteral(":/sleek/%1.svg").arg(name));
		if (!file.open(QIODevice::ReadOnly))
			return {};
		const QByteArray svg = file.readAll();

		QIcon icon;
		icon.addPixmap(RenderIcon(svg, QColor(p.textSoft)), QIcon::Normal, QIcon::Off);
		icon.addPixmap(RenderIcon(svg, QColor(p.accentTxt)), QIcon::Normal, QIcon::On);
		icon.addPixmap(RenderIcon(svg, QColor(p.text)), QIcon::Active, QIcon::Off);
		icon.addPixmap(RenderIcon(svg, QColor(p.textOff)), QIcon::Disabled, QIcon::Off);
		icon.addPixmap(RenderIcon(svg, QColor(p.textOff)), QIcon::Disabled, QIcon::On);
		return icon;
	}

	ccColor::Rgba ToRgba(const QColor& c)
	{
		return ccColor::Rgba(static_cast<ColorCompType>(c.red()), static_cast<ColorCompType>(c.green()), static_cast<ColorCompType>(c.blue()), ccColor::MAX);
	}

	QString FormatDistance(double value_m)
	{
		if (value_m < 1.0)
			return QObject::tr("%1 cm").arg(value_m * 100.0, 0, 'g', 4);
		return QObject::tr("%1 m").arg(value_m, 0, 'g', 6);
	}

	//! Removes the entities that are children of other entities of the set
	ccHObject::Container TopLevel(const ccHObject::Container& entities)
	{
		ccHObject::Container out;
		for (ccHObject* entity : entities)
		{
			bool hasAncestor = false;
			for (ccHObject* other : entities)
			{
				if (other != entity && other->isAncestorOf(entity))
				{
					hasAncestor = true;
					break;
				}
			}
			if (!hasAncestor)
				out.push_back(entity);
		}
		return out;
	}

	QDoubleSpinBox* MakeSpin(double min, double max, double value, int decimals, const QString& suffix)
	{
		QDoubleSpinBox* spin = new QDoubleSpinBox;
		spin->setRange(min, max);
		spin->setDecimals(decimals);
		spin->setValue(value);
		spin->setSuffix(suffix);
		spin->setMinimumWidth(110);
		spin->setKeyboardTracking(false);
		return spin;
	}

	QPushButton* MakePrimary(const QString& text)
	{
		QPushButton* button = new QPushButton(text);
		button->setObjectName("sleekPrimary");
		button->setDefault(true);
		return button;
	}

	QWidget* MakePanel(const QString& title, const QString& hint, QVBoxLayout*& layout)
	{
		QWidget* panel = new QWidget;
		layout         = new QVBoxLayout(panel);
		layout->setContentsMargins(12, 10, 12, 12);
		layout->setSpacing(8);
		QLabel* titleLabel = new QLabel(title);
		titleLabel->setObjectName("sleekPanelTitle");
		layout->addWidget(titleLabel);
		if (!hint.isEmpty())
		{
			QLabel* hintLabel = new QLabel(hint);
			hintLabel->setObjectName("sleekHint");
			hintLabel->setWordWrap(true);
			hintLabel->setMinimumWidth(250);
			layout->addWidget(hintLabel);
		}
		return panel;
	}

	//! Builds a 3D polyline from a run of vertices
	ccPolyline* MakePolyline(const std::vector<CCVector3>& points, bool closed)
	{
		if (points.size() < 2)
			return nullptr;
		ccPointCloud* vertices = new ccPointCloud("vertices");
		ccPolyline*   poly     = new ccPolyline(vertices);
		poly->addChild(vertices);
		unsigned count = static_cast<unsigned>(points.size());
		if (!vertices->reserve(count) || !poly->reserve(count))
		{
			delete poly;
			return nullptr;
		}
		for (unsigned i = 0; i < count; ++i)
		{
			vertices->addPoint(points[i]);
			poly->addPointIndex(i);
		}
		poly->setClosed(closed);
		vertices->setEnabled(false);
		return poly;
	}
} // namespace

ccSleekUI::ccSleekUI(MainWindow* win, Ui::MainWindow* ui)
    : QObject(win)
    , m_win(win)
    , m_ui(ui)
    , m_undo(new ccSleekUndo(win))
{
	QSettings settings;
	settings.beginGroup(S_GROUP);
	m_theme = settings.value(S_THEME, LIGHT).toInt() == DARK ? DARK : LIGHT;
	settings.endGroup();

	buildActions();
	buildTopBar();
	buildRail();
	buildDocks();
	buildMenus();

	// the history is meaningless once the DB has been emptied
	connect(m_win->db(), &ccDBRoot::dbIsEmpty, this, [this]()
	        {
		        if (m_undo->busy == 0 && !m_undo->recording())
			        m_undo->clear(); });

	setTheme(m_theme);

	if (qEnvironmentVariableIsSet("CC_SLEEK_SELFTEST"))
	{
		QTimer::singleShot(2500, this, &ccSleekUI::runSelfTest);
	}
}

/*** Theme ***/

void ccSleekUI::setTheme(Theme theme)
{
	m_theme = theme;

	QSettings settings;
	settings.beginGroup(S_GROUP);
	settings.setValue(S_THEME, static_cast<int>(m_theme));
	settings.endGroup();

	const Palette& p = GetPalette(m_theme);

	// 'Fusion' is the only style that follows the palette and the style sheet everywhere
	// (the native macOS style ignores most of them)
	static bool s_styleSet = false;
	if (!s_styleSet)
	{
		QApplication::setStyle(QStyleFactory::create("Fusion"));
		s_styleSet = true;
	}
	QApplication::setPalette(BuildQPalette(p));
	qApp->setStyleSheet(BuildStyleSheet(p));

	refreshIcons();
	applyThemeToViewport();

	if (m_actTheme)
	{
		m_actTheme->setText(m_theme == DARK ? tr("Light mode") : tr("Dark mode"));
		m_actTheme->setToolTip(m_theme == DARK ? tr("Switch to light mode") : tr("Switch to dark mode"));
		setThemedIcon(m_actTheme, m_theme == DARK ? "sun" : "moon");
	}
}

void ccSleekUI::toggleTheme()
{
	setTheme(m_theme == DARK ? LIGHT : DARK);
}

void ccSleekUI::applyThemeToViewport()
{
	const Palette& p = GetPalette(m_theme);

	QSettings settings;
	settings.beginGroup(S_GROUP);
	QColor background = QColor(settings.value(m_theme == DARK ? "BackgroundDark" : "BackgroundLight", QString(p.viewport)).toString());
	settings.endGroup();
	if (!background.isValid())
		background = QColor(p.viewport);

	// text, bounding-boxes, etc. must contrast with the background (whatever the theme)
	bool   darkBackground = background.lightness() < 128;
	QColor foreground     = darkBackground ? QColor("#E3E6EB") : QColor("#2A2F38");

	ccGui::ParamStruct params     = ccGui::Parameters();
	params.backgroundCol          = ccColor::Rgbub(static_cast<ColorCompType>(background.red()), static_cast<ColorCompType>(background.green()), static_cast<ColorCompType>(background.blue()));
	params.drawBackgroundGradient = false;
	params.textDefaultCol         = ToRgba(foreground);
	params.pointsDefaultCol       = ToRgba(foreground);
	params.bbDefaultCol           = ToRgba(QColor(p.accent));
	params.labelBackgroundCol     = ToRgba(darkBackground ? QColor("#2D3138") : QColor("#FFFFFF"));
	ccGui::Set(params);

	m_win->redrawAll();
}

void ccSleekUI::chooseBackgroundColor()
{
	const ccColor::Rgbub& current = ccGui::Parameters().backgroundCol;
	QColor                color   = QColorDialog::getColor(QColor(current.r, current.g, current.b), m_win, tr("Viewport background"));
	if (!color.isValid())
		return;

	QSettings settings;
	settings.beginGroup(S_GROUP);
	settings.setValue(m_theme == DARK ? "BackgroundDark" : "BackgroundLight", color.name());
	settings.endGroup();

	applyThemeToViewport();
}

QAction* ccSleekUI::themedAction(const QString& iconName, const QString& text, const QString& tip)
{
	QAction* action = new QAction(text, this);
	action->setToolTip(tip);
	action->setStatusTip(tip);
	setThemedIcon(action, iconName);
	return action;
}

void ccSleekUI::setThemedIcon(QAction* action, const QString& iconName)
{
	for (ThemedIcon& ti : m_themedIcons)
	{
		if (ti.action == action)
		{
			ti.name = iconName;
			action->setIcon(MakeIcon(iconName, GetPalette(m_theme)));
			return;
		}
	}
	m_themedIcons.push_back({action, iconName});
	action->setIcon(MakeIcon(iconName, GetPalette(m_theme)));
}

void ccSleekUI::refreshIcons()
{
	const Palette& p = GetPalette(m_theme);
	for (ThemedIcon& ti : m_themedIcons)
		ti.action->setIcon(MakeIcon(ti.name, p));
}

/*** Construction ***/

void ccSleekUI::buildActions()
{
	// undo / redo
	m_actUndo = themedAction("undo", tr("Undo"), tr("Undo the last step"));
	m_actUndo->setShortcut(QKeySequence::Undo);
	m_actRedo = themedAction("redo", tr("Redo"), tr("Redo the step that was just undone"));
	m_actRedo->setShortcuts({QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), QKeySequence(Qt::CTRL | Qt::Key_Y)});
	connect(m_actUndo, &QAction::triggered, m_undo->stack(), &QUndoStack::undo);
	connect(m_actRedo, &QAction::triggered, m_undo->stack(), &QUndoStack::redo);
	auto updateUndo = [this]()
	{
		QUndoStack* stack  = m_undo->stack();
		bool        frozen = m_rail && !m_actSelect->isEnabled();
		m_actUndo->setEnabled(!frozen && stack->canUndo());
		m_actRedo->setEnabled(!frozen && stack->canRedo());
		m_actUndo->setText(stack->canUndo() ? tr("Undo %1").arg(stack->undoText()) : tr("Undo"));
		m_actRedo->setText(stack->canRedo() ? tr("Redo %1").arg(stack->redoText()) : tr("Redo"));
		m_actUndo->setToolTip(m_actUndo->text());
		m_actRedo->setToolTip(m_actRedo->text());
	};
	connect(m_undo->stack(), &QUndoStack::indexChanged, this, updateUndo);
	connect(m_undo->stack(), &QUndoStack::cleanChanged, this, updateUndo);
	m_actUndo->setEnabled(false);
	m_actRedo->setEnabled(false);

	// theme and background
	m_actTheme = themedAction("moon", tr("Dark mode"), tr("Switch to dark mode"));
	connect(m_actTheme, &QAction::triggered, this, &ccSleekUI::toggleTheme);
	m_actBg = themedAction("background", tr("Background"), tr("Choose the viewport background colour"));
	connect(m_actBg, &QAction::triggered, this, &ccSleekUI::chooseBackgroundColor);

	// the standard actions that we reuse get a matching icon
	setThemedIcon(m_ui->actionOpen, "open");
	setThemedIcon(m_ui->actionSave, "save");
	setThemedIcon(m_ui->actionGlobalZoom, "fit");
	setThemedIcon(m_ui->actionSetViewTop, "viewtop");
	setThemedIcon(m_ui->actionSetViewFront, "viewfront");
	setThemedIcon(m_ui->actionSetViewIso1, "viewiso");
}

void ccSleekUI::buildTopBar()
{
	m_topBar = new QToolBar(tr("Main bar"), m_win);
	m_topBar->setObjectName("sleekTopBar");
	m_topBar->setMovable(false);
	m_topBar->setIconSize(QSize(20, 20));
	m_topBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

	m_topBar->addAction(m_ui->actionOpen);
	m_topBar->addAction(m_ui->actionSave);
	m_topBar->addSeparator();
	m_topBar->addAction(m_actUndo);
	m_topBar->addAction(m_actRedo);
	m_topBar->addSeparator();
	m_ui->actionGlobalZoom->setToolTip(tr("Zoom to fit everything"));
	m_ui->actionSetViewTop->setToolTip(tr("Top view"));
	m_ui->actionSetViewFront->setToolTip(tr("Front view"));
	m_ui->actionSetViewIso1->setToolTip(tr("3D view"));
	m_topBar->addAction(m_ui->actionGlobalZoom);
	m_topBar->addAction(m_ui->actionSetViewTop);
	m_topBar->addAction(m_ui->actionSetViewFront);
	m_topBar->addAction(m_ui->actionSetViewIso1);

	QWidget* spacer = new QWidget;
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_topBar->addWidget(spacer);

	m_topBar->addAction(m_actBg);
	m_topBar->addAction(m_actTheme);

	m_win->addToolBar(Qt::TopToolBarArea, m_topBar);
}

void ccSleekUI::buildRail()
{
	m_rail = new QToolBar(tr("Tools"), m_win);
	m_rail->setObjectName("sleekRail");
	m_rail->setMovable(false);
	m_rail->setOrientation(Qt::Vertical);
	m_rail->setIconSize(QSize(24, 24));
	m_rail->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

	m_actSelect = themedAction("select", tr("Select"), tr("Select: click a cloud in the view or in the Tags panel"));
	m_actTrim   = themedAction("trim", tr("Trim"), tr("Trim: draw a shape, then keep what is inside or outside of it"));
	m_actClip   = themedAction("clip", tr("Clip box"), tr("Clip box: crop with an adjustable box"));
	m_actMove   = themedAction("move", tr("Move"), tr("Move: type a distance or drag in the view"));
	m_actRotate = themedAction("rotate", tr("Rotate"), tr("Rotate: type an angle or drag in the view"));
	m_actSub    = themedAction("subsample", tr("Subsample"), tr("Subsample: thin the cloud to a regular point spacing"));
	m_actTopo   = themedAction("contours", tr("Contours"), tr("Contours: topo lines at 1 m, 5 m, or any interval"));
	m_actMesh   = themedAction("mesh", tr("Mesh"), tr("Mesh: build a terrain surface from the cloud"));
	m_actFlows  = themedAction("workflows", tr("Workflows"), tr("Workflows: several steps in one click"));

	QActionGroup* group = new QActionGroup(this);
	for (QAction* action : {m_actSelect, m_actTrim, m_actClip, m_actMove, m_actRotate})
	{
		action->setCheckable(true);
		group->addAction(action);
	}
	m_actSelect->setChecked(true);

	m_rail->addAction(m_actSelect);
	m_rail->addSeparator();
	m_rail->addAction(m_actTrim);
	m_rail->addAction(m_actClip);
	m_rail->addAction(m_actMove);
	m_rail->addAction(m_actRotate);
	m_rail->addAction(m_actSub);
	m_rail->addSeparator();
	m_rail->addAction(m_actTopo);
	m_rail->addAction(m_actMesh);
	m_rail->addSeparator();
	m_rail->addAction(m_actFlows);

	connect(m_actTrim, &QAction::triggered, this, [this]()
	        { triggerOnTargets(m_ui->actionSegment); });
	connect(m_actClip, &QAction::triggered, this, [this]()
	        { triggerOnTargets(m_ui->actionCrossSection); });
	connect(m_actMesh, &QAction::triggered, this, [this]()
	        { mesh(); });

	// tools with a panel
	auto attachPanel = [this](QAction* action, QWidget* (ccSleekUI::*factory)(QMenu*))
	{
		QToolButton* button = qobject_cast<QToolButton*>(m_rail->widgetForAction(action));
		if (!button)
			return;
		QMenu*         menu         = new QMenu(button);
		QWidgetAction* widgetAction = new QWidgetAction(menu);
		widgetAction->setDefaultWidget((this->*factory)(menu));
		menu->addAction(widgetAction);
		button->setMenu(menu);
		button->setPopupMode(QToolButton::InstantPopup);
		// a panel is not a mode: the tool gets checked only if the interactive tool is started
		connect(menu, &QMenu::aboutToHide, this, [this]()
		        {
			        if (m_actSelect->isEnabled())
				        m_actSelect->setChecked(true); });
	};
	attachPanel(m_actMove, &ccSleekUI::makeMovePanel);
	attachPanel(m_actRotate, &ccSleekUI::makeRotatePanel);
	attachPanel(m_actSub, &ccSleekUI::makeSubsamplePanel);
	attachPanel(m_actTopo, &ccSleekUI::makeContoursPanel);

	if (QToolButton* button = qobject_cast<QToolButton*>(m_rail->widgetForAction(m_actFlows)))
	{
		button->setMenu(makeWorkflowsMenu());
		button->setPopupMode(QToolButton::InstantPopup);
	}

	m_win->addToolBar(Qt::LeftToolBarArea, m_rail);
}

void ccSleekUI::buildDocks()
{
	m_ui->DockableDBTree->setWindowTitle(tr("Tags"));
	m_ui->DockableProperties->setWindowTitle(tr("Entity info"));
	m_ui->dbTreeView->setToolTip(tr("Click the eye to show or hide an item. Alt-click it to show only that item."));
	m_ui->dbTreeView->viewport()->installEventFilter(this);

	m_history = new QDockWidget(tr("History"), m_win);
	m_history->setObjectName("sleekHistoryDock");
	QUndoView* view = new QUndoView(m_undo->stack(), m_history);
	view->setEmptyLabel(tr("Start"));
	view->setToolTip(tr("Click a step to go back to it"));
	m_history->setWidget(view);
	m_win->addDockWidget(Qt::RightDockWidgetArea, m_history);
}

void ccSleekUI::buildMenus()
{
	// Edit menu: undo / redo first
	QAction* first = m_ui->menuEdit->actions().isEmpty() ? nullptr : m_ui->menuEdit->actions().front();
	m_ui->menuEdit->insertAction(first, m_actUndo);
	m_ui->menuEdit->insertAction(first, m_actRedo);
	m_ui->menuEdit->insertSeparator(first);

	// Display menu: theme, background and panels
	QAction* firstDisplay = m_ui->menuDisplay->actions().isEmpty() ? nullptr : m_ui->menuDisplay->actions().front();
	m_ui->menuDisplay->insertAction(firstDisplay, m_actTheme);
	m_ui->menuDisplay->insertAction(firstDisplay, m_actBg);
	QAction* historyToggle = m_history->toggleViewAction();
	historyToggle->setText(tr("History panel"));
	m_ui->menuDisplay->insertAction(firstDisplay, historyToggle);
	QAction* resetLayout = new QAction(tr("Reset to the simple layout"), this);
	connect(resetLayout, &QAction::triggered, this, &ccSleekUI::applyDefaultLayout);
	m_ui->menuDisplay->insertAction(firstDisplay, resetLayout);
	m_ui->menuDisplay->insertSeparator(firstDisplay);

	// plain names for the standard entries
	m_ui->actionSegment->setText(tr("Trim (segment)"));
	m_ui->actionCrossSection->setText(tr("Clip box (cross section)"));
	m_ui->actionTranslateRotate->setText(tr("Move / rotate"));
	m_ui->actionRasterize->setText(tr("Rasterize and contours (advanced)"));
}

void ccSleekUI::onLayoutRestored()
{
	QSettings settings;
	settings.beginGroup(S_GROUP);
	int version = settings.value(S_LAYOUT_VERSION, 0).toInt();
	settings.endGroup();

	if (version < LAYOUT_VERSION)
	{
		applyDefaultLayout();

		settings.beginGroup(S_GROUP);
		settings.setValue(S_LAYOUT_VERSION, LAYOUT_VERSION);
		settings.endGroup();
	}

	// whatever the restored state, these two can't be lost
	m_rail->setVisible(true);
	m_topBar->setVisible(true);
}

void ccSleekUI::applyDefaultLayout()
{
	// the standard toolbars remain available in 'Display > Toolbars'
	m_ui->toolBarMainTools->setVisible(false);
	m_ui->toolBarSFTools->setVisible(false);
	m_ui->toolBarView->setVisible(false);
	if (m_win->m_pluginUIManager)
	{
		if (m_win->m_pluginUIManager->mainPluginToolbar())
			m_win->m_pluginUIManager->mainPluginToolbar()->setVisible(false);
		if (m_win->m_pluginUIManager->glFiltersToolbar())
			m_win->m_pluginUIManager->glFiltersToolbar()->setVisible(false);
		for (QToolBar* toolbar : m_win->m_pluginUIManager->additionalPluginToolbars())
			toolbar->setVisible(false);
	}
	m_ui->DockableConsole->setVisible(false);

	m_rail->setVisible(true);
	m_topBar->setVisible(true);

	// panels on the right, like the SketchUp trays
	m_win->addDockWidget(Qt::RightDockWidgetArea, m_ui->DockableDBTree);
	m_win->addDockWidget(Qt::RightDockWidgetArea, m_ui->DockableProperties);
	m_win->addDockWidget(Qt::RightDockWidgetArea, m_history);
	m_ui->DockableDBTree->setVisible(true);
	m_ui->DockableProperties->setVisible(true);
	m_history->setVisible(true);
	m_win->resizeDocks({m_ui->DockableDBTree, m_ui->DockableProperties, m_history}, {300, 300, 300}, Qt::Horizontal);
	m_win->resizeDocks({m_ui->DockableDBTree, m_ui->DockableProperties, m_history}, {320, 260, 160}, Qt::Vertical);
}

void ccSleekUI::setFrozen(bool state)
{
	if (!m_rail)
		return;

	for (QAction* action : {m_actSelect, m_actTrim, m_actClip, m_actMove, m_actRotate, m_actSub, m_actTopo, m_actMesh, m_actFlows})
	{
		if (action)
			action->setEnabled(!state);
	}

	if (!state && m_actSelect)
		m_actSelect->setChecked(true);

	QUndoStack* stack = m_undo->stack();
	m_actUndo->setEnabled(!state && stack->canUndo());
	m_actRedo->setEnabled(!state && stack->canRedo());
}

/*** Panels ***/

QWidget* ccSleekUI::makeMovePanel(QMenu* menu)
{
	QVBoxLayout* layout = nullptr;
	QWidget*     panel  = MakePanel(tr("Move"), tr("Type how far to move the selection along each axis."), layout);

	QFormLayout*    form = new QFormLayout;
	QDoubleSpinBox* dx   = MakeSpin(-1.0e7, 1.0e7, 0.0, 3, tr(" m"));
	QDoubleSpinBox* dy   = MakeSpin(-1.0e7, 1.0e7, 0.0, 3, tr(" m"));
	QDoubleSpinBox* dz   = MakeSpin(-1.0e7, 1.0e7, 0.0, 3, tr(" m"));
	form->addRow(tr("X (red)"), dx);
	form->addRow(tr("Y (green)"), dy);
	form->addRow(tr("Z (blue, up)"), dz);
	layout->addLayout(form);

	QHBoxLayout* buttons = new QHBoxLayout;
	QPushButton* drag    = new QPushButton(tr("Drag in view"));
	QPushButton* apply   = MakePrimary(tr("Move"));
	buttons->addWidget(drag);
	buttons->addStretch();
	buttons->addWidget(apply);
	layout->addLayout(buttons);

	connect(apply, &QPushButton::clicked, this, [=]()
	        {
		        menu->close();
		        moveBy(dx->value(), dy->value(), dz->value()); });
	connect(drag, &QPushButton::clicked, this, [=]()
	        {
		        menu->close();
		        startTransformTool(ccGraphicalTransformationTool::PRESET_MOVE); });
	return panel;
}

QWidget* ccSleekUI::makeRotatePanel(QMenu* menu)
{
	QVBoxLayout* layout = nullptr;
	QWidget*     panel  = MakePanel(tr("Rotate"), tr("Rotates the selection about its centre. Positive angles turn counter-clockwise."), layout);

	QHBoxLayout*  axisRow   = new QHBoxLayout;
	QButtonGroup* axisGroup = new QButtonGroup(panel);
	axisRow->addWidget(new QLabel(tr("About")));
	const QString axisNames[3] = {tr("X (red)"), tr("Y (green)"), tr("Z (up)")};
	for (int i = 0; i < 3; ++i)
	{
		QPushButton* button = new QPushButton(axisNames[i]);
		button->setCheckable(true);
		button->setChecked(i == 2);
		axisGroup->addButton(button, i);
		axisRow->addWidget(button);
	}
	layout->addLayout(axisRow);

	QHBoxLayout*    angleRow = new QHBoxLayout;
	QDoubleSpinBox* angle    = MakeSpin(-360.0, 360.0, 90.0, 2, QStringLiteral("°"));
	angleRow->addWidget(new QLabel(tr("Angle")));
	angleRow->addWidget(angle);
	for (double preset : {-90.0, 90.0, 180.0})
	{
		QPushButton* button = new QPushButton(QStringLiteral("%1°").arg(preset));
		connect(button, &QPushButton::clicked, angle, [=]()
		        { angle->setValue(preset); });
		angleRow->addWidget(button);
	}
	layout->addLayout(angleRow);

	QHBoxLayout* buttons = new QHBoxLayout;
	QPushButton* drag    = new QPushButton(tr("Drag in view"));
	QPushButton* apply   = MakePrimary(tr("Rotate"));
	buttons->addWidget(drag);
	buttons->addStretch();
	buttons->addWidget(apply);
	layout->addLayout(buttons);

	connect(apply, &QPushButton::clicked, this, [=]()
	        {
		        menu->close();
		        rotateBy(axisGroup->checkedId(), angle->value()); });
	connect(drag, &QPushButton::clicked, this, [=]()
	        {
		        menu->close();
		        startTransformTool(ccGraphicalTransformationTool::PRESET_ROTATE); });
	return panel;
}

QWidget* ccSleekUI::makeSubsamplePanel(QMenu* menu)
{
	QVBoxLayout* layout = nullptr;
	QWidget*     panel  = MakePanel(tr("Subsample"), tr("Thins the cloud so that points are at least this far apart. The original is kept, hidden."), layout);

	QHBoxLayout* presets = new QHBoxLayout;
	for (double spacing : {0.05, 0.10, 0.25, 0.50})
	{
		QPushButton* button = new QPushButton(FormatDistance(spacing));
		connect(button, &QPushButton::clicked, this, [=]()
		        {
			        menu->close();
			        subsample(spacing); });
		presets->addWidget(button);
	}
	layout->addLayout(presets);

	QHBoxLayout*    custom  = new QHBoxLayout;
	QDoubleSpinBox* spacing = MakeSpin(0.001, 1000.0, 1.0, 3, tr(" m"));
	QPushButton*    apply   = MakePrimary(tr("Subsample"));
	custom->addWidget(new QLabel(tr("Spacing")));
	custom->addWidget(spacing);
	custom->addStretch();
	custom->addWidget(apply);
	layout->addLayout(custom);
	connect(apply, &QPushButton::clicked, this, [=]()
	        {
		        menu->close();
		        subsample(spacing->value()); });

	QPushButton* more = new QPushButton(tr("More options"));
	more->setFlat(true);
	layout->addWidget(more, 0, Qt::AlignLeft);
	connect(more, &QPushButton::clicked, this, [=]()
	        {
		        menu->close();
		        triggerOnTargets(m_ui->actionSubsample); });
	return panel;
}

QWidget* ccSleekUI::makeContoursPanel(QMenu* menu)
{
	QVBoxLayout* layout = nullptr;
	QWidget*     panel  = MakePanel(tr("Contours"), tr("Draws topo lines at a regular height interval. Every fifth line is drawn thicker."), layout);

	QCheckBox* saveDXF = new QCheckBox(tr("Also save a DXF file for SketchUp"));

	auto run = [=](double interval)
	{
		menu->close();
		ccHObject::Container lines = contours(interval);
		if (saveDXF->isChecked() && !lines.empty())
			exportForSketchUp(lines, "dxf", lines.front()->getName());
	};

	QHBoxLayout* presets = new QHBoxLayout;
	for (double interval : {1.0, 5.0})
	{
		QPushButton* button = MakePrimary(tr("%1 m lines").arg(interval));
		button->setDefault(false);
		connect(button, &QPushButton::clicked, this, [=]()
		        { run(interval); });
		presets->addWidget(button);
	}
	layout->addLayout(presets);

	QHBoxLayout*    custom   = new QHBoxLayout;
	QDoubleSpinBox* interval = MakeSpin(0.01, 1000.0, 0.5, 2, tr(" m"));
	QPushButton*    apply    = new QPushButton(tr("Generate"));
	custom->addWidget(new QLabel(tr("Other interval")));
	custom->addWidget(interval);
	custom->addStretch();
	custom->addWidget(apply);
	layout->addLayout(custom);
	connect(apply, &QPushButton::clicked, this, [=]()
	        { run(interval->value()); });

	layout->addWidget(saveDXF);

	QPushButton* more = new QPushButton(tr("More options"));
	more->setFlat(true);
	layout->addWidget(more, 0, Qt::AlignLeft);
	connect(more, &QPushButton::clicked, this, [=]()
	        {
		        menu->close();
		        triggerOnTargets(m_ui->actionRasterize); });
	return panel;
}

QMenu* ccSleekUI::makeWorkflowsMenu()
{
	QMenu* menu = new QMenu(m_win);

	menu->addSection(tr("Topo lines"));
	connect(menu->addAction(tr("Topo lines every 1 m")), &QAction::triggered, this, [this]()
	        { contours(1.0); });
	connect(menu->addAction(tr("Topo lines every 5 m")), &QAction::triggered, this, [this]()
	        { contours(5.0); });

	menu->addSection(tr("Scan to SketchUp"));
	connect(menu->addAction(tr("Subsample to 25 cm, 1 m topo lines, save DXF")), &QAction::triggered, this, [this]()
	        { workflowTopo(0.25, 1.0, true); });
	connect(menu->addAction(tr("Subsample to 50 cm, 5 m topo lines, save DXF")), &QAction::triggered, this, [this]()
	        { workflowTopo(0.50, 5.0, true); });
	connect(menu->addAction(tr("Subsample to 25 cm, mesh, save OBJ")), &QAction::triggered, this, [this]()
	        { workflowMesh(0.25, true); });

	menu->addSeparator();
	connect(menu->addAction(tr("Custom workflow...")), &QAction::triggered, this, [this]()
	        {
		        QSettings settings;
		        settings.beginGroup(S_GROUP);

		        QDialog dialog(m_win);
		        dialog.setWindowTitle(tr("Custom workflow"));
		        QVBoxLayout*    layout   = new QVBoxLayout(&dialog);
		        QFormLayout*    form     = new QFormLayout;
		        QDoubleSpinBox* spacing  = MakeSpin(0.0, 1000.0, settings.value("CustomSpacing", 0.25).toDouble(), 3, tr(" m"));
		        QRadioButton*   topo     = new QRadioButton(tr("Topo lines"));
		        QRadioButton*   meshOut  = new QRadioButton(tr("Mesh"));
		        QDoubleSpinBox* interval = MakeSpin(0.01, 1000.0, settings.value("CustomInterval", 1.0).toDouble(), 2, tr(" m"));
		        QCheckBox*      save     = new QCheckBox(tr("Save a file for SketchUp (DXF for lines, OBJ for a mesh)"));
		        bool            wantMesh = settings.value("CustomMesh", false).toBool();
		        topo->setChecked(!wantMesh);
		        meshOut->setChecked(wantMesh);
		        save->setChecked(settings.value("CustomSave", true).toBool());
		        interval->setEnabled(!wantMesh);
		        connect(topo, &QRadioButton::toggled, interval, &QWidget::setEnabled);
		        QHBoxLayout* output = new QHBoxLayout;
		        output->addWidget(topo);
		        output->addWidget(meshOut);
		        output->addStretch();
		        form->addRow(tr("1. Subsample to (0 = skip)"), spacing);
		        form->addRow(tr("2. Create"), output);
		        form->addRow(tr("Topo line interval"), interval);
		        layout->addLayout(form);
		        layout->addWidget(save);
		        QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Cancel);
		        QPushButton*      run = MakePrimary(tr("Run"));
		        box->addButton(run, QDialogButtonBox::AcceptRole);
		        connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		        connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		        layout->addWidget(box);

		        if (dialog.exec() != QDialog::Accepted)
			        return;

		        settings.setValue("CustomSpacing", spacing->value());
		        settings.setValue("CustomInterval", interval->value());
		        settings.setValue("CustomMesh", meshOut->isChecked());
		        settings.setValue("CustomSave", save->isChecked());
		        settings.endGroup();

		        if (meshOut->isChecked())
			        workflowMesh(spacing->value(), save->isChecked());
		        else
			        workflowTopo(spacing->value(), interval->value(), save->isChecked()); });

	return menu;
}

/*** Helpers ***/

void ccSleekUI::notify(const QString& message)
{
	m_win->statusBar()->showMessage(message, 8000);
	ccLog::Print(message);
}

ccHObject::Container ccSleekUI::targets(bool cloudsOnly /*=true*/)
{
	ccHObject::Container out;
	for (ccHObject* entity : m_win->getSelectedEntities())
	{
		if (!cloudsOnly || entity->isA(CC_TYPES::POINT_CLOUD))
			out.push_back(entity);
	}

	if (out.empty() && !m_win->haveSelection() && m_win->dbRootObject())
	{
		// nothing is selected: we work on the clouds that are displayed
		ccHObject::Container clouds;
		m_win->dbRootObject()->filterChildren(clouds, true, CC_TYPES::POINT_CLOUD, true);
		for (ccHObject* entity : clouds)
		{
			ccGenericPointCloud* cloud = static_cast<ccGenericPointCloud*>(entity);
			if (entity->isBranchEnabled() && entity->isVisible() && !ccGenericMesh::IsCloudVerticesOfMesh(cloud) && !ccPolyline::IsCloudVerticesOfPolyline(cloud))
				out.push_back(entity);
		}
		if (!out.empty())
			m_win->db()->selectEntities(out);
	}

	if (out.empty())
		notify(cloudsOnly ? tr("Open or select a point cloud first.") : tr("Select something first."));

	return out;
}

void ccSleekUI::triggerOnTargets(QAction* action)
{
	if (targets(false).empty())
	{
		m_actSelect->setChecked(true);
		return;
	}
	// the main window updates its selection asynchronously: we let it do so first
	QTimer::singleShot(0, this, [this, action]()
	                   {
		                   if (action->isEnabled())
			                   action->trigger();
		                   if (m_actSelect->isEnabled())
			                   m_actSelect->setChecked(true); });
}

void ccSleekUI::startTransformTool(int preset)
{
	if (targets(false).empty())
		return;
	QTimer::singleShot(0, this, [this, preset]()
	                   {
		                   if (!m_ui->actionTranslateRotate->isEnabled())
			                   return;
		                   m_ui->actionTranslateRotate->trigger();
		                   if (m_win->m_transTool && !m_actSelect->isEnabled())
		                   {
			                   m_win->m_transTool->setPreset(static_cast<ccGraphicalTransformationTool::Preset>(preset));
			                   (preset == ccGraphicalTransformationTool::PRESET_ROTATE ? m_actRotate : m_actMove)->setChecked(true);
		                   } });
}

bool ccSleekUI::eventFilter(QObject* obj, QEvent* event)
{
	// Alt + click on an item of the Tags panel: show only this item (or everything again)
	if (obj == m_ui->dbTreeView->viewport() && event->type() == QEvent::MouseButtonPress)
	{
		QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
		if (mouseEvent->button() == Qt::LeftButton && (mouseEvent->modifiers() & Qt::AltModifier))
		{
			QModelIndex index = m_ui->dbTreeView->indexAt(mouseEvent->pos());
			ccHObject*  item  = index.isValid() ? static_cast<ccHObject*>(index.internalPointer()) : nullptr;
			if (item && item->getParent())
			{
				ccHObject* parent    = item->getParent();
				bool       soloAlready = item->isEnabled();
				for (unsigned i = 0; i < parent->getChildrenNumber() && soloAlready; ++i)
				{
					if (parent->getChild(i) != item && parent->getChild(i)->isEnabled())
						soloAlready = false;
				}
				for (unsigned i = 0; i < parent->getChildrenNumber(); ++i)
				{
					ccHObject* sibling = parent->getChild(i);
					sibling->setEnabled(soloAlready || sibling == item);
					sibling->prepareDisplayForRefresh_recursive();
				}
				m_ui->dbTreeView->viewport()->update();
				m_win->refreshAll();
				m_win->updateUI();
				return true;
			}
		}
	}
	return QObject::eventFilter(obj, event);
}

bool ccSleekUI::deleteSelection()
{
	return m_undo->deleteEntities(m_win->getSelectedEntities());
}

/*** Operations ***/

void ccSleekUI::moveBy(double dx, double dy, double dz)
{
	if (dx == 0.0 && dy == 0.0 && dz == 0.0)
		return;
	ccHObject::Container entities = TopLevel(targets(false));
	if (entities.empty())
		return;

	ccGLMatrixd mat;
	mat.toIdentity();
	mat.setTranslation(CCVector3d(dx, dy, dz));
	m_undo->pushTransform(tr("Move"), entities, mat, false);
}

void ccSleekUI::rotateBy(int axis, double angle_deg)
{
	if (angle_deg == 0.0 || axis < 0 || axis > 2)
		return;
	ccHObject::Container entities = TopLevel(targets(false));
	if (entities.empty())
		return;

	ccBBox box;
	for (ccHObject* entity : entities)
		box += entity->getBB_recursive();
	if (!box.isValid())
		return;
	CCVector3d center = box.getCenter();

	CCVector3d axisVec(0, 0, 0);
	axisVec.u[axis] = 1.0;

	ccGLMatrixd mat;
	mat.initFromParameters(CCCoreLib::DegreesToRadians(angle_deg), axisVec, CCVector3d(0, 0, 0));
	CCVector3d rotatedCenter = center;
	mat.applyRotation(rotatedCenter);
	mat.setTranslation(center - rotatedCenter);

	static const char* s_axisNames[3] = {"X", "Y", "Z"};
	m_undo->pushTransform(tr("Rotate %1° about %2").arg(angle_deg).arg(s_axisNames[axis]), entities, mat, false);
}

ccHObject::Container ccSleekUI::subsample(double spacing, const ccHObject::Container* targetEntities /*=nullptr*/)
{
	ccHObject::Container results;
	ccHObject::Container sources = targetEntities ? *targetEntities : targets();
	if (sources.empty() || spacing <= 0.0)
		return results;

	ccSleekUndoScope scope(m_undo, tr("Subsample to %1").arg(FormatDistance(spacing)), sources);

	ccProgressDialog pDlg(false, m_win);
	pDlg.setAutoClose(false);

	for (ccHObject* entity : sources)
	{
		if (!entity->isA(CC_TYPES::POINT_CLOUD))
			continue;
		ccPointCloud* cloud = static_cast<ccPointCloud*>(entity);

		CCCoreLib::CloudSamplingTools::SFModulationParams modParams(false);
		CCCoreLib::ReferenceCloud*                        sampled = CCCoreLib::CloudSamplingTools::resampleCloudSpatially(cloud, static_cast<PointCoordinateType>(spacing), modParams, nullptr, &pDlg);
		if (!sampled)
		{
			ccLog::Warning(tr("[Subsample] Failed to subsample cloud '%1'").arg(cloud->getName()));
			continue;
		}

		ccPointCloud* newCloud = cloud->partialClone(sampled);
		delete sampled;
		if (!newCloud)
		{
			ccLog::Error(tr("Not enough memory"));
			break;
		}

		newCloud->setName(cloud->getName() + QStringLiteral(" (%1)").arg(FormatDistance(spacing)));
		newCloud->copyGlobalShiftAndScale(*cloud);
		newCloud->setDisplay(cloud->getDisplay());
		if (cloud->getParent())
			cloud->getParent()->addChild(newCloud);
		cloud->setEnabled(false);
		cloud->prepareDisplayForRefresh_recursive();
		m_win->addToDB(newCloud, false, true, false, false);
		newCloud->prepareDisplayForRefresh();
		results.push_back(newCloud);

		notify(tr("Subsampled '%1': %L2 points kept out of %L3").arg(cloud->getName()).arg(newCloud->size()).arg(cloud->size()));
	}
	pDlg.close();

	if (!results.empty())
		m_win->db()->selectEntities(results);
	m_win->refreshAll();
	m_win->updateUI();

	return results;
}

ccHObject::Container ccSleekUI::contours(double interval, const ccHObject::Container* targetEntities /*=nullptr*/)
{
	ccHObject::Container results;
	ccHObject::Container sources = targetEntities ? *targetEntities : targets();
	if (sources.empty() || interval <= 0.0)
		return results;

	ccSleekUndoScope scope(m_undo, tr("Topo lines every %1").arg(FormatDistance(interval)), sources);

	for (ccHObject* entity : sources)
	{
		if (!entity->isA(CC_TYPES::POINT_CLOUD))
			continue;
		ccPointCloud* cloud = static_cast<ccPointCloud*>(entity);
		if (cloud->size() < 3)
			continue;

		ccBBox box = cloud->getOwnBB();
		if (!box.isValid())
			continue;

		// grid step: fine enough for smooth lines, but not finer than what the cloud can support
		CCVector3 diag       = box.getDiagVec();
		double    area       = std::max(static_cast<double>(diag.x) * diag.y, 1.0e-9);
		double    ptSpacing  = std::sqrt(area / cloud->size());
		double    gridStep   = std::max({interval / 4.0, 2.0 * ptSpacing, std::sqrt(area / 4.0e6)});
		unsigned  gridWidth  = 0;
		unsigned  gridHeight = 0;
		if (!ccRasterGrid::ComputeGridSize(2, box, gridStep, gridWidth, gridHeight) || gridWidth < 2 || gridHeight < 2)
		{
			ccLog::Warning(tr("[Contours] Cloud '%1' is too small for this interval").arg(cloud->getName()));
			continue;
		}

		ccRasterGrid grid;
		CCVector3d   minCorner = box.minCorner();
		if (!grid.init(gridWidth, gridHeight, gridStep, minCorner))
		{
			ccLog::Error(tr("Not enough memory"));
			continue;
		}

		// small holes are filled, big ones are left empty (no invented ground)
		ccRasterGrid::DelaunayInterpolationParams interpParams;
		interpParams.maxEdgeLength = 25.0 * gridStep;
		{
			ccProgressDialog pDlg(true, m_win);
			if (!grid.fillWith(cloud, 2, ccRasterGrid::PROJ_AVERAGE_VALUE, ccRasterGrid::InterpolationType::DELAUNAY, &interpParams, ccRasterGrid::INVALID_PROJECTION_TYPE, &pDlg))
			{
				ccLog::Warning(tr("[Contours] Failed to rasterize cloud '%1'").arg(cloud->getName()));
				continue;
			}
		}

		// light smoothing (3x3 mean) so that the lines don't follow the scan noise
		double minH = std::numeric_limits<double>::max();
		double maxH = std::numeric_limits<double>::lowest();
		{
			std::vector<double> smoothed(static_cast<size_t>(gridWidth) * gridHeight, std::numeric_limits<double>::quiet_NaN());
			for (unsigned j = 0; j < gridHeight; ++j)
			{
				for (unsigned i = 0; i < gridWidth; ++i)
				{
					if (!std::isfinite(grid.rows[j][i].h))
						continue;
					double   sum   = 0.0;
					unsigned count = 0;
					for (int dj = -1; dj <= 1; ++dj)
					{
						for (int di = -1; di <= 1; ++di)
						{
							int jj = static_cast<int>(j) + dj;
							int ii = static_cast<int>(i) + di;
							if (jj < 0 || ii < 0 || jj >= static_cast<int>(gridHeight) || ii >= static_cast<int>(gridWidth))
								continue;
							double h = grid.rows[jj][ii].h;
							if (std::isfinite(h))
							{
								sum += h;
								++count;
							}
						}
					}
					smoothed[static_cast<size_t>(j) * gridWidth + i] = sum / count;
				}
			}
			for (unsigned j = 0; j < gridHeight; ++j)
			{
				for (unsigned i = 0; i < gridWidth; ++i)
				{
					double h = smoothed[static_cast<size_t>(j) * gridWidth + i];
					if (std::isfinite(h))
					{
						grid.rows[j][i].h = h;
						minH              = std::min(minH, h);
						maxH              = std::max(maxH, h);
					}
				}
			}
		}
		if (minH > maxH)
			continue;

		ccContourLinesGenerator::Parameters params;
		params.step                      = interval;
		params.startAltitude             = std::ceil(minH / interval) * interval;
		params.maxAltitude               = maxH;
		params.minVertexCount            = 6;
		params.projectContourOnAltitudes = false;
		params.emptyCellsValue           = minH - 1.0;
		params.ignoreBorders             = true;
		params.parentWidget              = m_win;
		if (params.startAltitude > params.maxAltitude)
		{
			notify(tr("'%1' spans less than %2 in height: no topo line at this interval.").arg(cloud->getName(), FormatDistance(interval)));
			continue;
		}

		std::vector<ccPolyline*> rawLines;
		if (!ccContourLinesGenerator::GenerateContourLines(&grid, CCVector2d(grid.minCorner.x, grid.minCorner.y), params, rawLines))
		{
			ccLog::Warning(tr("[Contours] Failed to generate the topo lines of '%1'").arg(cloud->getName()));
			for (ccPolyline* poly : rawLines)
				delete poly;
			continue;
		}

		// we cut the lines where they run along empty areas (they are not real contours there)
		auto cellIsSafe = [&](const CCVector3* P)
		{
			int ci = static_cast<int>(std::floor((P->x - grid.minCorner.x) / gridStep));
			int cj = static_cast<int>(std::floor((P->y - grid.minCorner.y) / gridStep));
			for (int dj = -1; dj <= 1; ++dj)
			{
				for (int di = -1; di <= 1; ++di)
				{
					int jj = cj + dj;
					int ii = ci + di;
					if (jj < 0 || ii < 0 || jj >= static_cast<int>(gridHeight) || ii >= static_cast<int>(gridWidth))
						continue;
					if (!std::isfinite(grid.rows[jj][ii].h))
						return false;
				}
			}
			return true;
		};

		ccHObject* group = new ccHObject(tr("Topo lines %1 (%2)").arg(FormatDistance(interval), cloud->getName()));
		const ccColor::Rgb lineColor(37, 99, 235);
		unsigned           lineCount = 0;
		for (ccPolyline* raw : rawLines)
		{
			double altitude = raw->getMetaData(ccPolyline::MetaKeyConstAltitude()).toDouble();
			bool   isIndex  = (std::llround(altitude / interval) % 5) == 0;

			std::vector<ccPolyline*> parts;
			std::vector<CCVector3>   run;
			bool                     wasCut = false;
			unsigned                 count  = raw->size();
			for (unsigned k = 0; k < count; ++k)
			{
				const CCVector3* P = raw->getPoint(k);
				if (cellIsSafe(P))
				{
					run.push_back(*P);
				}
				else
				{
					wasCut = true;
					if (run.size() >= 3)
						parts.push_back(MakePolyline(run, false));
					run.clear();
				}
			}
			if (!wasCut)
			{
				parts.push_back(raw);
				raw = nullptr;
			}
			else if (run.size() >= 3)
			{
				parts.push_back(MakePolyline(run, false));
			}
			delete raw;

			for (ccPolyline* poly : parts)
			{
				if (!poly)
					continue;
				poly->setName(tr("%1 m").arg(altitude, 0, 'f', interval < 1.0 ? 2 : 0));
				poly->setMetaData(ccPolyline::MetaKeyConstAltitude(), altitude);
				poly->copyGlobalShiftAndScale(*cloud);
				poly->setColor(lineColor);
				poly->showColors(true);
				poly->setWidth(isIndex ? 2 : 0);
				group->addChild(poly);
				++lineCount;
			}
		}

		if (lineCount == 0)
		{
			delete group;
			notify(tr("No topo line found for '%1' at this interval.").arg(cloud->getName()));
			continue;
		}

		group->setDisplay_recursive(cloud->getDisplay());
		m_win->addToDB(group, false, false, false, false);
		results.push_back(group);
		notify(tr("%1 topo lines every %2, from %3 m to %4 m").arg(lineCount).arg(FormatDistance(interval)).arg(params.startAltitude).arg(maxH, 0, 'f', 1));
	}

	m_win->refreshAll();
	m_win->updateUI();
	return results;
}

ccHObject::Container ccSleekUI::mesh(const ccHObject::Container* targetEntities /*=nullptr*/)
{
	ccHObject::Container results;
	ccHObject::Container sources = targetEntities ? *targetEntities : targets();
	if (sources.empty())
		return results;

	ccSleekUndoScope scope(m_undo, tr("Mesh"), sources);

	ccProgressDialog pDlg(false, m_win);
	pDlg.setAutoClose(false);
	pDlg.setWindowTitle(tr("Mesh"));
	pDlg.setInfo(tr("Building the surface..."));
	pDlg.setRange(0, 0);
	pDlg.show();
	QApplication::processEvents();

	for (ccHObject* entity : sources)
	{
		if (!entity->isA(CC_TYPES::POINT_CLOUD))
			continue;
		ccPointCloud* cloud = static_cast<ccPointCloud*>(entity);
		if (cloud->size() < 3)
			continue;

		// no triangle across the big gaps
		ccBBox    box       = cloud->getOwnBB();
		CCVector3 diag      = box.getDiagVec();
		double    ptSpacing = std::sqrt(std::max(static_cast<double>(diag.x) * diag.y, 1.0e-9) / cloud->size());

		ccMesh* newMesh = ccMesh::Triangulate(cloud, CCCoreLib::DELAUNAY_2D_AXIS_ALIGNED, false, static_cast<PointCoordinateType>(20.0 * ptSpacing), 2);
		if (!newMesh)
		{
			ccLog::Warning(tr("[Mesh] Failed to mesh cloud '%1'").arg(cloud->getName()));
			continue;
		}

		newMesh->setName(tr("Mesh (%1)").arg(cloud->getName()));
		newMesh->computeNormals(true);
		newMesh->showNormals(true);
		newMesh->showColors(cloud->hasColors());
		cloud->setVisible(false); // can't disable the cloud as the mesh is its child
		cloud->addChild(newMesh);
		cloud->prepareDisplayForRefresh_recursive();
		m_win->addToDB(newMesh, false, true, false, false);
		results.push_back(newMesh);
		notify(tr("Mesh of '%1': %L2 triangles").arg(cloud->getName()).arg(newMesh->size()));
	}
	pDlg.close();

	if (!results.empty())
		m_win->db()->selectEntities(results);
	m_win->refreshAll();
	m_win->updateUI();
	return results;
}

bool ccSleekUI::exportForSketchUp(const ccHObject::Container& entities, const QString& extension, const QString& suggestedName)
{
	if (entities.empty())
		return false;

	FileIOFilter::Shared filter = FileIOFilter::FindBestFilterForExtension(extension);
	if (!filter)
	{
		ccLog::Error(tr("This build can't save %1 files (the I/O plugin is missing).").arg(extension.toUpper()));
		return false;
	}

	QSettings settings;
	settings.beginGroup(S_GROUP);
	QString folder = settings.value("ExportFolder", QDir::homePath()).toString();

	QString baseName = suggestedName;
	baseName.replace(QRegularExpression("[^A-Za-z0-9_.-]+"), "_");
	QString filename = QDir(folder).filePath(baseName + '.' + extension);

	if (qEnvironmentVariableIsSet("CC_SLEEK_SELFTEST"))
	{
		filename = QDir(qEnvironmentVariable("CC_SLEEK_SELFTEST")).filePath(baseName + '.' + extension);
	}
	else
	{
		filename = QFileDialog::getSaveFileName(m_win, tr("Save for SketchUp"), filename, QStringLiteral("%1 (*.%2)").arg(extension.toUpper(), extension));
		if (filename.isEmpty())
			return false;
		settings.setValue("ExportFolder", QFileInfo(filename).absolutePath());
	}
	settings.endGroup();

	// several entities are saved together
	ccHObject  container("export");
	ccHObject* toSave = entities.front();
	if (entities.size() > 1)
	{
		for (ccHObject* entity : entities)
			container.addChild(entity, ccHObject::DP_NONE);
		toSave = &container;
	}

	FileIOFilter::SaveParameters params;
	params.alwaysDisplaySaveDialog = false;
	params.parentWidget            = m_win;
	CC_FILE_ERROR result           = FileIOFilter::SaveToFile(toSave, filename, params, filter);
	container.detachAllChildren();

	if (result != CC_FERR_NO_ERROR)
	{
		FileIOFilter::DisplayErrorMessage(result, "saving", filename);
		return false;
	}

	notify(tr("Saved %1. In SketchUp, use File > Import.").arg(QDir::toNativeSeparators(filename)));
	return true;
}

void ccSleekUI::workflowTopo(double spacing, double interval, bool exportDXF)
{
	ccHObject::Container sources = targets();
	if (sources.empty())
		return;

	ccHObject::Container lines;
	{
		ccSleekUndoScope     scope(m_undo, tr("Workflow: topo lines every %1").arg(FormatDistance(interval)), sources);
		ccHObject::Container thinned = spacing > 0.0 ? subsample(spacing, &sources) : sources;
		if (thinned.empty())
			return;
		lines = contours(interval, &thinned);
	}

	if (exportDXF && !lines.empty())
		exportForSketchUp(lines, "dxf", lines.front()->getName());
}

void ccSleekUI::workflowMesh(double spacing, bool exportOBJ)
{
	ccHObject::Container sources = targets();
	if (sources.empty())
		return;

	ccHObject::Container meshes;
	{
		ccSleekUndoScope     scope(m_undo, tr("Workflow: mesh"), sources);
		ccHObject::Container thinned = spacing > 0.0 ? subsample(spacing, &sources) : sources;
		if (thinned.empty())
			return;
		meshes = mesh(&thinned);
	}

	if (exportOBJ && !meshes.empty())
		exportForSketchUp(meshes, "obj", meshes.front()->getName());
}
