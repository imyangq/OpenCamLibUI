#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include <array>
#include <vtkSmartPointer.h>
#include <vtkActor.h>

#include <vtkTextActor.h>
#include <vtkTextProperty.h>

class vtkGenericOpenGLRenderWindow;
class vtkRenderer;
class QAction;
class QLabel;
class vtkOrientationMarkerWidget;
class vtkAxesActor;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void importSTL();
    void closeCurrentFile();
    void openCamSettings();
    void runCam();
    void toggleWaterline();
    void toggleDropPath();
    void toggleModel();
    void clearScene();

private:
    void addAxesWidget();
    void saveCamSettingsToJson(const QString &stlPath, const QJsonObject &camSettings);
    QJsonObject loadCamSettingsFromJson(const QString &stlPath);
    void renderCamPaths(const std::vector<std::array<double, 3>> &points);
    void saveCamPathsToJson(const QString &stlPath,
                            const std::vector<std::array<double, 3>> &camPaths);
    void renderWaterlineLoops(const std::vector<std::vector<std::array<double, 3>>> &loops);
    void clearCamRenderData();
    void loadCamPathsIfExists(const QString &stlPath);

    QAction *showModelAction;
    vtkSmartPointer<vtkActor> modelActor;

    QAction *showWaterlineAction;
    QAction *showDropPathAction;
    std::vector<vtkSmartPointer<vtkActor>> waterlineActors;
    std::vector<vtkSmartPointer<vtkActor>> dropPathActors;

    vtkGenericOpenGLRenderWindow *renderWindow;
    vtkRenderer *renderer;
    vtkSmartPointer<vtkTextActor> modelInfoText;

    QAction *importAction;
    QAction *camSettingsAction;
    QAction *runCamAction;
    QLabel *statusLabel;
    vtkOrientationMarkerWidget *axesWidget;
    vtkAxesActor *axes;

    QString lastStlPath;
    std::array<double, 6> aabb;
};
