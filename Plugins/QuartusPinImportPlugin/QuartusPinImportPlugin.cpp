//-----------------------------------------------------------------------------
// File: QuartusPinImportPlugin.cpp
//-----------------------------------------------------------------------------
// Project: Kactus 2
// Author: Esko Pekkarinen
// Date: 19.11.2013
//
// Description:
// Kactus2 plugin for Quartus II pin import.
//-----------------------------------------------------------------------------


#include "QuartusPinImportPlugin.h"
#include "QuartusPinSyntax.h"

#include <KactusAPI/include/ImportColors.h>

#include <IPXACTmodels/Component/Component.h>
#include <IPXACTmodels/Component/Port.h>

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::QuartusPinImportPlugin()
//-----------------------------------------------------------------------------
QuartusPinImportPlugin::QuartusPinImportPlugin(): highlighter_(0)
{
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::~QuartusPinImportPlugin()
//-----------------------------------------------------------------------------
QuartusPinImportPlugin::~QuartusPinImportPlugin()
{
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getName()
//-----------------------------------------------------------------------------
QString QuartusPinImportPlugin::getName() const
{
    return tr("Quartus II Pin Import");
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getVersion()
//-----------------------------------------------------------------------------
QString QuartusPinImportPlugin::getVersion() const
{
    return tr("1.1");
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getDescription()
//-----------------------------------------------------------------------------
QString QuartusPinImportPlugin::getDescription() const
{
    return tr("Imports ports from Quartus pin file.");
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getVendor()
//-----------------------------------------------------------------------------
QString QuartusPinImportPlugin::getVendor() const
{
    return tr("TUT");
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getLicense()
//-----------------------------------------------------------------------------
QString QuartusPinImportPlugin::getLicense() const
{
    return tr("GPL2");
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getLicenseHolder()
//-----------------------------------------------------------------------------
QString QuartusPinImportPlugin::getLicenseHolder() const
{
    return tr("Public");
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getSettingsWidget()
//-----------------------------------------------------------------------------
QWidget* QuartusPinImportPlugin::getSettingsWidget()
{
    return nullptr;
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getProgramRequirements()
//-----------------------------------------------------------------------------
QList<IPlugin::ExternalProgramRequirement> QuartusPinImportPlugin::getProgramRequirements()
{
    return QList<IPlugin::ExternalProgramRequirement>();
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::setHighlighter()
//-----------------------------------------------------------------------------
void QuartusPinImportPlugin::setHighlighter(Highlighter* highlighter)
{
    highlighter_ = highlighter;
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getSupportedFileTypes()
//-----------------------------------------------------------------------------
QStringList QuartusPinImportPlugin::getSupportedFileTypes() const
{
    return QStringList("quartusPinFile");
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getCompatibilityWarnings()
//-----------------------------------------------------------------------------
QString QuartusPinImportPlugin::getCompatibilityWarnings() const
{
    return QString();
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getFileComponents()
//-----------------------------------------------------------------------------
QStringList QuartusPinImportPlugin::getFileComponents(QString const& /*input*/) const
{
    return QStringList();
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::getComponentName()
//-----------------------------------------------------------------------------
//     virtual QStringList getComponentNames(QStringList const& fileComponents) const = 0;
QString QuartusPinImportPlugin::getComponentName(QString const& /*componentDeclaration*/) const
{
    return QString();
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::import()
//-----------------------------------------------------------------------------
void QuartusPinImportPlugin::import(QString const& input, QString const& /*componentDeclaration*/,
    QSharedPointer<Component> targetComponent)
{
    if (highlighter_)
    {
        highlighter_->applyFontColor(input, QColor("gray"));
    }

    //! The existing ports are changed to phantom in order to keep the component error-free.
    //! These ports can then be deleted later.
    foreach (QSharedPointer<Port> existingPort, *targetComponent->getPorts())
    {
        existingPort->setDirection(DirectionTypes::DIRECTION_PHANTOM);
    }

    foreach (QString line, input.split(QRegularExpression("(\\r\\n?|\\n\\r?)")))
    {
        if (QuartusPinSyntax::pinDefinition.match(line).hasMatch())
        {
            createPort(line, targetComponent);
        }
    }
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::createPort()
//-----------------------------------------------------------------------------
void QuartusPinImportPlugin::createPort(QString const& line, QSharedPointer<Component> targetComponent)
{
    if (highlighter_)
    {
        highlighter_->applyFontColor(line, Qt::black);
        highlighter_->applyHighlight(line, ImportColors::PORT);
    }

    QRegularExpressionMatch lineMatch = QuartusPinSyntax::pinDefinition.match(line);
    
    QString portName = lineMatch.captured(QuartusPinSyntax::LOCATION);

    QString direction = lineMatch.captured(QuartusPinSyntax::DIRECTION);
    DirectionTypes::Direction portDirection = parseDirection(direction);

    QString description = lineMatch.captured(QuartusPinSyntax::PINUSAGE);

    QSharedPointer<Port> port = targetComponent->getPort(portName);
    if (port.isNull())
    {
        QSharedPointer<Wire> portWire(new Wire());
        portWire->setDirection(DirectionTypes::DIRECTION_PHANTOM);

        port = QSharedPointer<Port>(new Port(portName));
        port->setWire(portWire);

        targetComponent->getPorts()->append(port);
    }

    port->setDirection(portDirection);
    port->setDescription(description);
    port->setPortSize(1);
}

//-----------------------------------------------------------------------------
// Function: QuartusPinImportPlugin::parseDirection()
//-----------------------------------------------------------------------------
DirectionTypes::Direction QuartusPinImportPlugin::parseDirection(QString const& direction)
{
    if (QRegularExpression("input", QRegularExpression::CaseInsensitiveOption).match(direction).hasMatch())
    {
        return DirectionTypes::IN;
    }
    else if (QRegularExpression("output", QRegularExpression::CaseInsensitiveOption).match(direction).hasMatch())
    {
        return DirectionTypes::OUT;
    }
    else if (QRegularExpression("bidir|power|gnd",
        QRegularExpression::CaseInsensitiveOption).match(direction).hasMatch())
    {
        return DirectionTypes::INOUT;
    }
    else
    {
        return DirectionTypes::DIRECTION_PHANTOM;
    }
}
