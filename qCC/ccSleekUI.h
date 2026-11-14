#pragma once

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

// The "sleek" interface: a SketchUp-style layer on top of the standard
// CloudCompare main window.
//
//  - a tool rail (Select, Trim, Clip, Move, Rotate, Subsample, Contours,
//    Mesh, Workflows) with plainly named tools and typed-value panels
//  - a light and a (real) dark theme, viewport included
//  - Tags (the DB tree, with eye toggles) and History panels
//  - undo / redo (see ccSleekUndo)
//  - quick automated workflows (scan to topo lines / mesh for SketchUp)
//
// Everything lives in this class so that the footprint on the upstream
// code stays small (a few hooks in MainWindow).

#include <QColor>
#include <QObject>
#include <QString>

#include <ccHObject.h>

#include <vector>

class MainWindow;
class ccSleekUndo;
class ccPointCloud;
class QAction;
class QDockWidget;
class QMenu;
class QToolBar;
class QToolButton;
class QWidget;

namespace Ui
{
	class MainWindow;
}

class ccSleekUI : public QObject
{
	Q_OBJECT

  public:
	enum Theme
	{
		LIGHT = 0,
		DARK  = 1
	};

	ccSleekUI(MainWindow* win, Ui::MainWindow* ui);

	ccSleekUndo* undo()
	{
		return m_undo;
	}

	//! To be called once the main window has restored its previous layout
	void onLayoutRestored();

	//! To be called when the main window (un)freezes its UI (i.e. when a tool starts or stops)
	void setFrozen(bool state);

	Theme theme() const
	{
		return m_theme;
	}
	void setTheme(Theme theme);
	void toggleTheme();

	//! Lets the user choose the viewport background color (remembered per theme)
	void chooseBackgroundColor();

	//! Undoable deletion of the selected entities (returns false if the standard deletion should be used instead)
	bool deleteSelection();

	// Direct operations (all of them are undoable)
	void moveBy(double dx, double dy, double dz);
	void rotateBy(int axis, double angle_deg);
	//! Spatial subsampling (min. distance between points)
	ccHObject::Container subsample(double spacing, const ccHObject::Container* targets = nullptr);
	//! Topo lines (contours) at a regular interval
	ccHObject::Container contours(double interval, const ccHObject::Container* targets = nullptr);
	//! 2.5D mesh (Delaunay, XY plane)
	ccHObject::Container mesh(const ccHObject::Container* targets = nullptr);

	// Workflows
	void workflowTopo(double spacing, double interval, bool exportDXF);
	void workflowMesh(double spacing, bool exportOBJ);

	//! Saves entities to a file readable by SketchUp (DXF for lines, OBJ for meshes)
	bool exportForSketchUp(const ccHObject::Container& entities, const QString& extension, const QString& suggestedName);

  private:
	void buildActions();
	void buildRail();
	void buildTopBar();
	void buildDocks();
	void buildMenus();
	void applyDefaultLayout();
	void applyThemeToViewport();
	void refreshIcons();
	void runSelfTest();

	QWidget* makeMovePanel(QMenu* menu);
	QWidget* makeRotatePanel(QMenu* menu);
	QWidget* makeSubsamplePanel(QMenu* menu);
	QWidget* makeContoursPanel(QMenu* menu);
	QMenu*   makeWorkflowsMenu();

	//! Adds an action whose icon follows the theme
	QAction* themedAction(const QString& iconName, const QString& text, const QString& tip);
	void     setThemedIcon(QAction* action, const QString& iconName);

	//! Returns the entities to work on: the selection or, if nothing is selected, the visible clouds
	ccHObject::Container targets(bool cloudsOnly = true);
	//! Makes sure something is selected, then triggers a standard action
	void triggerOnTargets(QAction* action);
	void startTransformTool(int preset);
	void notify(const QString& message);

	bool eventFilter(QObject* obj, QEvent* event) override;

	MainWindow*     m_win;
	Ui::MainWindow* m_ui;
	ccSleekUndo*    m_undo;
	Theme           m_theme = LIGHT;

	QToolBar*    m_rail      = nullptr;
	QToolBar*    m_topBar    = nullptr;
	QDockWidget* m_history   = nullptr;
	QAction*     m_actSelect = nullptr;
	QAction*     m_actTrim   = nullptr;
	QAction*     m_actClip   = nullptr;
	QAction*     m_actMove   = nullptr;
	QAction*     m_actRotate = nullptr;
	QAction*     m_actSub    = nullptr;
	QAction*     m_actTopo   = nullptr;
	QAction*     m_actMesh   = nullptr;
	QAction*     m_actFlows  = nullptr;
	QAction*     m_actTheme  = nullptr;
	QAction*     m_actBg     = nullptr;
	QAction*     m_actUndo   = nullptr;
	QAction*     m_actRedo   = nullptr;

	struct ThemedIcon
	{
		QAction* action;
		QString  name;
	};
	std::vector<ThemedIcon> m_themedIcons;
};
