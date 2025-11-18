#include "MainWindow.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QStatusBar>
#include <QLabel>
#include <QVTKOpenGLNativeWidget.h>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QCheckBox>
#include <QPushButton>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QFile>
#include <QTimer>
#include <QMessageBox>
#include <QJsonArray>

#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPolyData.h>
#include <vtkOutlineFilter.h>
#include <vtkBoundingBox.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkPoints.h>
#include <vtkPolyLine.h>
#include <vtkCellArray.h>
#include <vtkNamedColors.h>

#include <string>
#include <iostream>
#include <waterline.hpp>
#include <adaptivewaterline.hpp>
#include <pathdropcutter.hpp>
#include <adaptivepathdropcutter.hpp>
#include <stlsurf.hpp>
#include <stlreader.hpp>
#include <cylcutter.hpp>
#include <ballcutter.hpp>
#include <bullcutter.hpp>
#include <conecutter.hpp>
#include <point.hpp>
#include <line.hpp>
#include <path.hpp>

#include <vector>
#include <array>

#include <locale>
#include <codecvt>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      renderWindow(nullptr),
      renderer(nullptr),
      axesWidget(nullptr),
      axes(nullptr)
{
    QMenu *fileMenu = menuBar()->addMenu("File");
    importAction = new QAction("Open STL", this);
    fileMenu->addAction(importAction);
    connect(importAction, &QAction::triggered, this, &MainWindow::importSTL);

    QAction *closeAction = new QAction("Close STL", this);
    fileMenu->addAction(closeAction);
    connect(closeAction, &QAction::triggered, this, &MainWindow::closeCurrentFile);

    QMenu *camMenu = menuBar()->addMenu("CAM");
    camSettingsAction = new QAction("Settings", this);
    runCamAction = new QAction("Run", this);
    camMenu->addAction(camSettingsAction);
    camMenu->addAction(runCamAction);
    connect(camSettingsAction, &QAction::triggered, this, &MainWindow::openCamSettings);
    connect(runCamAction, &QAction::triggered, this, &MainWindow::runCam);

    QMenu *viewMenu = menuBar()->addMenu("View");
    showModelAction = new QAction("Show Model", this);
    showModelAction->setCheckable(true);
    showModelAction->setChecked(true);
    connect(showModelAction, &QAction::triggered, this, &MainWindow::toggleModel);
    viewMenu->addAction(showModelAction);

    showWaterlineAction = new QAction("Show Waterline", this);
    showWaterlineAction->setCheckable(true);
    showWaterlineAction->setChecked(false);
    connect(showWaterlineAction, &QAction::triggered, this, &MainWindow::toggleWaterline);
    viewMenu->addAction(showWaterlineAction);

    showDropPathAction = new QAction("Show Drop Path", this);
    showDropPathAction->setCheckable(true);
    showDropPathAction->setChecked(true);
    connect(showDropPathAction, &QAction::triggered, this, &MainWindow::toggleDropPath);
    viewMenu->addAction(showDropPathAction);

    statusLabel = new QLabel("Ready");
    statusBar()->addPermanentWidget(statusLabel);

    auto vtkWidget = new QVTKOpenGLNativeWidget(this);
    setCentralWidget(vtkWidget);

    renderWindow = vtkGenericOpenGLRenderWindow::New();
    vtkWidget->setRenderWindow(renderWindow);

    renderer = vtkRenderer::New();
    renderer->GradientBackgroundOn();
    renderer->SetBackground(0.8, 0.8, 0.8);
    renderer->SetBackground2(0.4, 0.4, 0.4);

    renderWindow->AddRenderer(renderer);

    modelInfoText = vtkSmartPointer<vtkTextActor>::New();
    modelInfoText->GetTextProperty()->SetFontFamilyToCourier();
    modelInfoText->GetTextProperty()->SetFontSize(14);
    modelInfoText->GetTextProperty()->SetColor(0.0, 0.0, 0.0);
    modelInfoText->GetTextProperty()->SetOpacity(1.0);
    modelInfoText->GetTextProperty()->SetJustificationToLeft();
    modelInfoText->GetTextProperty()->SetVerticalJustificationToTop();
    modelInfoText->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
    modelInfoText->SetPosition(0.01, 0.99);
    modelInfoText->SetVisibility(0);
    renderer->AddActor2D(modelInfoText);

    setWindowTitle("CAM Demo");
    resize(800, 600);
}

MainWindow::~MainWindow()
{
    if (axesWidget)
        axesWidget->Delete();
    if (axes)
        axes->Delete();
    renderer->Delete();
    renderWindow->Delete();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(100, this, [this]()
                       { this->addAxesWidget(); });
}

void MainWindow::closeCurrentFile()
{
    if (lastStlPath.isEmpty())
    {
        statusLabel->setText("No STL file to close.");
        return;
    }
    clearScene();
    for (auto &actor : dropPathActors)
        renderer->RemoveActor(actor);
    for (auto &actor : waterlineActors)
        renderer->RemoveActor(actor);
    if (modelActor)
    {
        renderer->RemoveActor(modelActor);
        modelActor = nullptr;
    }
    dropPathActors.clear();
    waterlineActors.clear();
    if (modelInfoText)
    {
        modelInfoText->SetVisibility(0);
        renderer->AddActor2D(modelInfoText);
    }
    lastStlPath.clear();
    renderWindow->Render();
    statusLabel->setText("STL file closed. View cleared.");
}

void MainWindow::addAxesWidget()
{
    if (axesWidget)
        return;

    auto vtkWidget = qobject_cast<QVTKOpenGLNativeWidget *>(centralWidget());
    if (!vtkWidget || !vtkWidget->interactor())
        return;

    axes = vtkAxesActor::New();
    axesWidget = vtkOrientationMarkerWidget::New();
    axesWidget->SetOrientationMarker(axes);
    axesWidget->SetInteractor(vtkWidget->interactor());
    axesWidget->SetViewport(0.0, 0.0, 0.2, 0.2);
    axesWidget->SetEnabled(1);
    axesWidget->InteractiveOff();
    renderWindow->Render();
}

void MainWindow::importSTL()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open STL File", "", "STL Files (*.stl)");
    if (fileName.isEmpty())
        return;

    if (!lastStlPath.isEmpty())
    {
        clearCamRenderData();

        if (modelActor)
        {
            renderer->RemoveActor(modelActor);
            modelActor = nullptr;
        }

        if (modelInfoText)
            modelInfoText->SetVisibility(0);

        renderer->ResetCamera();
        renderer->SetBackground(0.8, 0.8, 0.8);
        renderer->SetBackground2(0.4, 0.4, 0.4);

        showModelAction->setChecked(true);
        showWaterlineAction->setChecked(false);
        showDropPathAction->setChecked(true);

        statusLabel->setText("Previous model cleared.");
    }

    lastStlPath = fileName;

    vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
    reader->SetFileName(fileName.toStdString().c_str());
    reader->Update();

    vtkSmartPointer<vtkPolyData> polyData = reader->GetOutput();
    if (!polyData || polyData->GetNumberOfPoints() == 0)
    {
        QMessageBox::warning(this, "Error", "Failed to load STL model.");
        return;
    }

    vtkIdType numPoints = polyData->GetNumberOfPoints();
    vtkIdType numPolys = polyData->GetNumberOfPolys();

    double bounds[6];
    polyData->GetBounds(bounds);
    aabb = {bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]};

    vtkSmartPointer<vtkOutlineFilter> outlineFilter = vtkSmartPointer<vtkOutlineFilter>::New();
    outlineFilter->SetInputData(polyData);

    vtkSmartPointer<vtkPolyDataMapper> outlineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    outlineMapper->SetInputConnection(outlineFilter->GetOutputPort());

    vtkSmartPointer<vtkActor> outlineActor = vtkSmartPointer<vtkActor>::New();
    outlineActor->SetMapper(outlineMapper);
    outlineActor->GetProperty()->SetColor(0, 0, 0);
    outlineActor->GetProperty()->SetLineWidth(2.0);

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(reader->GetOutputPort());

    modelActor = vtkSmartPointer<vtkActor>::New();
    modelActor->SetMapper(mapper);

    renderer->AddActor(modelActor);
    renderer->AddActor(outlineActor);

    renderer->ResetCamera();
    renderWindow->Render();

    QString infoText = QString(
                           "Model: %1\n"
                           "Points: %2\n"
                           "Faces: %3\n"
                           "X Range: [%4, %5]\n"
                           "Y Range: [%6, %7]\n"
                           "Z Range: [%8, %9]")
                           .arg(QFileInfo(fileName).fileName())
                           .arg(numPoints)
                           .arg(numPolys)
                           .arg(bounds[0], 0, 'f', 3)
                           .arg(bounds[1], 0, 'f', 3)
                           .arg(bounds[2], 0, 'f', 3)
                           .arg(bounds[3], 0, 'f', 3)
                           .arg(bounds[4], 0, 'f', 3)
                           .arg(bounds[5], 0, 'f', 3);

    modelInfoText->SetInput(infoText.toStdString().c_str());
    modelInfoText->SetVisibility(1);
    renderWindow->Render();

    if (modelInfoText)
    {
        if (!renderer->HasViewProp(modelInfoText))
            renderer->AddActor2D(modelInfoText);
    }

    loadCamPathsIfExists(fileName);
}

void MainWindow::toggleModel()
{
    if (!modelActor)
    {
        statusLabel->setText("No model loaded.");
        return;
    }

    bool visible = showModelAction->isChecked();
    modelActor->SetVisibility(visible ? 1 : 0);

    renderWindow->Render();
    statusLabel->setText(visible ? "Model shown." : "Model hidden.");
}

void MainWindow::openCamSettings()
{
    if (lastStlPath.isEmpty())
    {
        statusLabel->setText("Please import an STL file first.");
        return;
    }

    QJsonObject lastSettings = loadCamSettingsFromJson(lastStlPath);

    QDialog dialog(this);
    dialog.setWindowTitle("Settings");
    QFormLayout *layout = new QFormLayout(&dialog);

    QComboBox *cutterType = new QComboBox();
    cutterType->addItems({"CylCutter", "BallCutter", "BullCutter", "ConeCutter"});
    layout->addRow("Cutter Type:", cutterType);

    QLineEdit *diameter = new QLineEdit();
    QLineEdit *cornerRadius = new QLineEdit();
    QLineEdit *length = new QLineEdit();

    diameter->setValidator(new QDoubleValidator(0, 9999, 3));
    cornerRadius->setValidator(new QDoubleValidator(0, 9999, 3));
    length->setValidator(new QDoubleValidator(0, 9999, 3));

    layout->addRow("Diameter:", diameter);
    layout->addRow("Corner Radius:", cornerRadius);
    layout->addRow("Length:", length);

    QLineEdit *zHeight = new QLineEdit();
    QLineEdit *sampleStep = new QLineEdit();
    QLineEdit *minSampleStep = new QLineEdit();

    zHeight->setValidator(new QDoubleValidator(-9999, 9999, 3));
    sampleStep->setValidator(new QDoubleValidator(0, 9999, 3));
    minSampleStep->setValidator(new QDoubleValidator(0, 9999, 3));

    layout->addRow("Drop Z-Ref:", zHeight);
    layout->addRow("Sample Step:", sampleStep);

    QCheckBox *adaptiveCheck = new QCheckBox("Enable Adaptive Sampling");
    layout->addRow(adaptiveCheck);
    layout->addRow("Min Sample Step:", minSampleStep);

    cutterType->setCurrentText(lastSettings.contains("cutter_type") ? lastSettings["cutter_type"].toString() : "CylCutter");
    diameter->setText(QString::number(lastSettings.contains("diameter") ? lastSettings["diameter"].toDouble() : 0.4));
    cornerRadius->setText(QString::number(lastSettings.contains("corner_radius") ? lastSettings["corner_radius"].toDouble() : 0.05));
    length->setText(QString::number(lastSettings.contains("length") ? lastSettings["length"].toDouble() : 10.0));
    zHeight->setText(QString::number(lastSettings.contains("z_height") ? lastSettings["z_height"].toDouble() : 0.0));
    sampleStep->setText(QString::number(lastSettings.contains("sample_step") ? lastSettings["sample_step"].toDouble() : 0.1));
    adaptiveCheck->setChecked(lastSettings.contains("adaptive") ? lastSettings["adaptive"].toBool() : false);
    minSampleStep->setText(QString::number(lastSettings.contains("min_sample_step") ? lastSettings["min_sample_step"].toDouble() : 0.05));

    QObject::connect(cutterType, &QComboBox::currentTextChanged, [&](const QString &type)
                     { cornerRadius->setEnabled(type == "BullCutter" || type == "ConeCutter"); });
    cornerRadius->setEnabled(cutterType->currentText() == "BullCutter" || cutterType->currentText() == "ConeCutter");

    QObject::connect(adaptiveCheck, &QCheckBox::toggled, [&](bool checked)
                     { minSampleStep->setEnabled(checked); });
    minSampleStep->setEnabled(adaptiveCheck->isChecked());

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton("OK");
    QPushButton *cancelBtn = new QPushButton("Cancel");
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addRow(btnLayout);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, [&]()
                     {
                         QJsonObject camSettings;
                         camSettings["cutter_type"] = cutterType->currentText();
                         camSettings["diameter"] = diameter->text().toDouble();
                         if (cornerRadius->isEnabled())
                             camSettings["corner_radius"] = cornerRadius->text().toDouble();
                         camSettings["length"] = length->text().toDouble();
                         camSettings["z_height"] = zHeight->text().toDouble();
                         camSettings["sample_step"] = sampleStep->text().toDouble();
                         camSettings["adaptive"] = adaptiveCheck->isChecked();
                         if (adaptiveCheck->isChecked())
                             camSettings["min_sample_step"] = minSampleStep->text().toDouble();

                         saveCamSettingsToJson(lastStlPath, camSettings);
                         dialog.accept(); });

    dialog.exec();
}

void MainWindow::saveCamSettingsToJson(const QString &stlPath, const QJsonObject &camSettings)
{
    QFileInfo info(stlPath);
    QString baseName = info.completeBaseName();
    QString folderPath = info.absolutePath() + "/" + baseName;

    QDir dir(folderPath);
    if (!dir.exists())
        dir.mkpath(".");

    QString outputPath = folderPath + "/cam_input.json";

    QJsonDocument doc(camSettings);
    QFile file(outputPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        statusLabel->setText("Saved: " + outputPath);
    }
    else
    {
        statusLabel->setText("Failed to save JSON.");
    }
}

QJsonObject MainWindow::loadCamSettingsFromJson(const QString &stlPath)
{
    QFileInfo info(stlPath);
    QString baseName = info.completeBaseName();
    QString path = info.absolutePath() + "/" + baseName + "/cam_input.json";

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        QString fallback = info.absolutePath() + "/cam_input.json";
        file.setFileName(fallback);
        if (!file.open(QIODevice::ReadOnly))
        {
            return {};
        }
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
    {
        QMessageBox::warning(this, "Error", "Invalid JSON format.");
        return {};
    }
    return doc.object();
}

void MainWindow::renderCamPaths(const std::vector<std::array<double, 3>> &points)
{
    if (points.empty())
        return;

    vtkSmartPointer<vtkPoints> vtkPts = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();

    vtkIdType prevId = -1;
    for (size_t i = 0; i < points.size(); ++i)
    {
        vtkIdType id = vtkPts->InsertNextPoint(points[i][0], points[i][1], points[i][2]);
        if (i > 0)
        {
            vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New();
            polyLine->GetPointIds()->SetNumberOfIds(2);
            polyLine->GetPointIds()->SetId(0, prevId);
            polyLine->GetPointIds()->SetId(1, id);
            lines->InsertNextCell(polyLine);
        }
        prevId = id;
    }

    vtkSmartPointer<vtkPolyData> lineData = vtkSmartPointer<vtkPolyData>::New();
    lineData->SetPoints(vtkPts);
    lineData->SetLines(lines);

    vtkSmartPointer<vtkPolyDataMapper> lineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    lineMapper->SetInputData(lineData);

    vtkSmartPointer<vtkActor> lineActor = vtkSmartPointer<vtkActor>::New();
    lineActor->SetMapper(lineMapper);
    lineActor->GetProperty()->SetColor(1.0, 0.0, 0.0);
    lineActor->GetProperty()->SetLineWidth(2.0);

    renderer->AddActor(lineActor);
    dropPathActors.push_back(lineActor);

    bool visible = showDropPathAction->isChecked();
    for (auto &actor : dropPathActors)
    {
        actor->SetVisibility(visible ? 1 : 0);
    }
    renderWindow->Render();
}

std::wstring string_to_wstring(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}

void MainWindow::runCam()
{
    if (lastStlPath.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please import STL file first.");
        return;
    }

    clearCamRenderData();

    QJsonObject config = loadCamSettingsFromJson(lastStlPath);
    if (config.isEmpty())
        return;

    statusLabel->setText("Running CAM...");

    std::vector<std::array<double, 3>> camPaths;
    ocl::STLSurf surface = ocl::STLSurf();
    ocl::STLReader(string_to_wstring(lastStlPath.toStdString()), surface);
    if (surface.size() == 0)
    {
        QMessageBox::warning(this, "Error", "Failed to load STL surface.");
        statusLabel->setText("CAM Run Failed.");
        return;
    }

    double xmin = aabb[0];
    double xmax = aabb[1];
    double ymin = aabb[2];
    double ymax = aabb[3];
    double zmin = aabb[4];
    double zmax = aabb[5];

    double diameter = config["diameter"].toDouble();
    double length = config["length"].toDouble();
    double dropZRef = config["z_height"].toDouble();
    double sampling = config["sample_step"].toDouble();
    bool adaptive = config["adaptive"].toBool();
    double minSampling = adaptive ? config["min_sample_step"].toDouble() : sampling;

    ocl::MillingCutter *millingCutter = nullptr;
    if (config["cutter_type"].toString() == "CylCutter")
    {
        millingCutter = new ocl::CylCutter(diameter, length);
    }
    else if (config["cutter_type"].toString() == "BallCutter")
    {
        millingCutter = new ocl::BallCutter(diameter, length);
    }
    else if (config["cutter_type"].toString() == "BullCutter")
    {
        double cornerRadius = config["corner_radius"].toDouble();
        millingCutter = new ocl::BullCutter(diameter, cornerRadius, length);
    }
    else if (config["cutter_type"].toString() == "ConeCutter")
    {
        double cornerRadius = config["corner_radius"].toDouble();
        millingCutter = new ocl::ConeCutter(diameter, cornerRadius, length);
    }

    std::vector<std::vector<std::array<double, 3>>> waterlineLoops;
    if (adaptive)
    {
        ocl::AdaptiveWaterline awl = ocl::AdaptiveWaterline();
        awl.setSTL(surface);
        awl.setCutter(millingCutter);
        awl.setSampling(sampling);
        awl.setMinSampling(minSampling);
        for (double h = zmin; h < zmax; h = h + sampling)
        {
            awl.reset();
            awl.setZ(h);
            awl.run();
            auto loops = awl.getLoops();
            for (const auto &loop : loops)
            {
                std::vector<std::array<double, 3>> loopPnts;
                for (const auto &pnt : loop)
                {
                    loopPnts.push_back({pnt.x, pnt.y, pnt.z});
                }
                waterlineLoops.push_back(loopPnts);
            }
        }
    }
    else
    {
        ocl::Waterline wl = ocl::Waterline();
        wl.setSTL(surface);
        wl.setCutter(millingCutter);
        wl.setSampling(sampling);
        for (double h = zmin; h < zmax; h = h + sampling)
        {
            wl.reset();
            wl.setZ(h);
            wl.run();
            auto loops = wl.getLoops();
            for (const auto &loop : loops)
            {
                std::vector<std::array<double, 3>> loopPnts;
                for (const auto &pnt : loop)
                {
                    loopPnts.push_back({pnt.x, pnt.y, pnt.z});
                }
                waterlineLoops.push_back(loopPnts);
            }
        }
    }

    ocl::Path path = ocl::Path();
    int i = 0;
    for (double y = ymin; y <= ymax; y = y + sampling)
    {
        bool ltr = ((int)i % 2) == 0;
        ocl::Point p1 = ocl::Point(ltr ? xmin : xmax, y, 0);
        ocl::Point p2 = ocl::Point(ltr ? xmax : xmin, y, 0);
        ocl::Line l = ocl::Line(p1, p2);
        path.append(l);
        ocl::Point p3 = ocl::Point(ltr ? xmax : xmin, y + sampling, 0);
        ocl::Line l2 = ocl::Line(p2, p3);
        path.append(l2);
        i++;
    }

    ocl::PathDropCutter pdc = ocl::PathDropCutter();
    pdc.setSTL(surface);
    pdc.setCutter(millingCutter);
    pdc.setPath(&path);
    pdc.setSampling(sampling);
    pdc.reset();
    pdc.setZ(dropZRef);
    pdc.run();
    auto points = pdc.getPoints();
    for (auto &p : points)
    {
        camPaths.push_back({p.x, p.y, p.z});
    }

    renderWaterlineLoops(waterlineLoops);
    delete millingCutter;
    millingCutter = nullptr;
    renderCamPaths(camPaths);
    saveCamPathsToJson(lastStlPath, camPaths);
    statusLabel->setText("CAM Run Completed.");
}

void MainWindow::saveCamPathsToJson(const QString &stlPath,
                                    const std::vector<std::array<double, 3>> &camPaths)
{
    if (camPaths.empty())
    {
        statusLabel->setText("No CAM paths to save.");
        return;
    }

    QFileInfo info(stlPath);
    QString baseName = info.completeBaseName();
    QString folderPath = info.absolutePath() + "/" + baseName;

    QDir dir(folderPath);
    if (!dir.exists())
        dir.mkpath(".");

    QString outputPath = folderPath + "/cam_paths.json";

    QJsonArray jsonPoints;
    for (const auto &p : camPaths)
    {
        QJsonObject pt;
        pt["x"] = p[0];
        pt["y"] = p[1];
        pt["z"] = p[2];
        jsonPoints.append(pt);
    }

    QJsonObject root;
    root["count"] = static_cast<int>(camPaths.size());
    root["points"] = jsonPoints;

    QJsonDocument doc(root);
    QFile file(outputPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        statusLabel->setText("CAM paths saved: " + outputPath);
    }
    else
    {
        statusLabel->setText("Failed to save CAM paths JSON.");
    }
}

void MainWindow::renderWaterlineLoops(const std::vector<std::vector<std::array<double, 3>>> &loops)
{
    if (loops.empty())
        return;

    for (const auto &loop : loops)
    {
        if (loop.size() < 2)
            continue;

        vtkSmartPointer<vtkPoints> vtkPts = vtkSmartPointer<vtkPoints>::New();
        vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();

        vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New();
        polyLine->GetPointIds()->SetNumberOfIds(loop.size() + 1);

        for (size_t i = 0; i < loop.size(); ++i)
        {
            vtkIdType pid = vtkPts->InsertNextPoint(loop[i][0], loop[i][1], loop[i][2]);
            polyLine->GetPointIds()->SetId(i, pid);
        }

        vtkIdType firstPid = 0;
        polyLine->GetPointIds()->SetId(loop.size(), firstPid);

        lines->InsertNextCell(polyLine);

        vtkSmartPointer<vtkPolyData> lineData = vtkSmartPointer<vtkPolyData>::New();
        lineData->SetPoints(vtkPts);
        lineData->SetLines(lines);

        vtkSmartPointer<vtkPolyDataMapper> lineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        lineMapper->SetInputData(lineData);

        vtkSmartPointer<vtkActor> lineActor = vtkSmartPointer<vtkActor>::New();
        lineActor->SetMapper(lineMapper);
        lineActor->GetProperty()->SetColor(0.0, 1.0, 0.0);
        lineActor->GetProperty()->SetLineWidth(1.5);

        renderer->AddActor(lineActor);
        waterlineActors.push_back(lineActor);
    }
    bool visible = showWaterlineAction->isChecked();
    for (auto &actor : waterlineActors)
    {
        actor->SetVisibility(visible ? 1 : 0);
    }
    renderWindow->Render();
    statusLabel->setText(QString("Rendered %1 Waterline loops (closed)").arg(loops.size()));
}

void MainWindow::toggleWaterline()
{
    bool visible = showWaterlineAction->isChecked();
    for (auto &actor : waterlineActors)
    {
        actor->SetVisibility(visible ? 1 : 0);
    }
    renderWindow->Render();
}

void MainWindow::toggleDropPath()
{
    bool visible = showDropPathAction->isChecked();
    for (auto &actor : dropPathActors)
    {
        actor->SetVisibility(visible ? 1 : 0);
    }
    renderWindow->Render();
}

void MainWindow::clearCamRenderData()
{
    for (auto &actor : dropPathActors)
        renderer->RemoveActor(actor);
    for (auto &actor : waterlineActors)
        renderer->RemoveActor(actor);

    dropPathActors.clear();
    waterlineActors.clear();

    renderWindow->Render();
    statusLabel->setText("Cleared previous CAM render data.");
}

void MainWindow::loadCamPathsIfExists(const QString &stlPath)
{
    QFileInfo info(stlPath);
    QString baseName = info.completeBaseName();
    QString folderPath = info.absolutePath() + "/" + baseName;
    QString jsonPath = folderPath + "/cam_paths.json";

    QFile file(jsonPath);
    if (!file.exists())
    {
        statusLabel->setText("No existing cam_paths.json found for this model.");
        return;
    }

    if (!file.open(QIODevice::ReadOnly))
    {
        statusLabel->setText("Failed to open cam_paths.json.");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
    {
        statusLabel->setText("Invalid cam_paths.json format.");
        return;
    }

    QJsonObject root = doc.object();
    if (!root.contains("points") || !root["points"].isArray())
    {
        statusLabel->setText("cam_paths.json missing 'points' field.");
        return;
    }

    QJsonArray pointsArray = root["points"].toArray();
    std::vector<std::array<double, 3>> points;
    points.reserve(pointsArray.size());
    for (const auto &val : pointsArray)
    {
        if (!val.isObject())
            continue;
        QJsonObject pt = val.toObject();
        if (pt.contains("x") && pt.contains("y") && pt.contains("z"))
        {
            points.push_back({pt["x"].toDouble(), pt["y"].toDouble(), pt["z"].toDouble()});
        }
    }

    if (points.empty())
    {
        statusLabel->setText("No valid path points found in cam_paths.json.");
        return;
    }

    renderCamPaths(points);

    bool visible = showDropPathAction && showDropPathAction->isChecked();
    for (auto &actor : dropPathActors)
        actor->SetVisibility(visible ? 1 : 0);

    renderWindow->Render();
    statusLabel->setText(QString("Loaded existing CAM paths (%1 points).").arg(points.size()));
}

void MainWindow::clearScene()
{
    if (!renderer)
        return;

    auto props = renderer->GetViewProps();

    props->InitTraversal();
    vtkProp *prop = nullptr;

    while ((prop = props->GetNextProp()) != nullptr)
    {
        if (prop == axes || prop == modelInfoText)
            continue;

        renderer->RemoveViewProp(prop);
    }

    dropPathActors.clear();
    waterlineActors.clear();
    modelActor = nullptr;

    renderWindow->Render();
}
