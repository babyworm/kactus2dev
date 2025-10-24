//-----------------------------------------------------------------------------
// File: MetaComponent.cpp
//-----------------------------------------------------------------------------
// Project: Kactus2
// Author: Janne Virtanen
// Date: 03.02.2017
//
// Description:
// Class used to parse relevant information from IP-XACT component for HDL generation.
//-----------------------------------------------------------------------------

#include "MetaComponent.h"

#include <KactusAPI/include/MessageMediator.h>

#include <IPXACTmodels/utilities/Search.h>

#include <IPXACTmodels/Component/RemapState.h>
#include <IPXACTmodels/Component/RemapPort.h>

#include <KactusAPI/include/ComponentParameterFinder.h>
#include <KactusAPI/include/MultipleParameterFinder.h>
#include <KactusAPI/include/ListParameterFinder.h>
#include <KactusAPI/include/ExpressionFormatter.h>

//-----------------------------------------------------------------------------
// Function: MetaComponent::MetaComponent
//-----------------------------------------------------------------------------
MetaComponent::MetaComponent(MessageMediator* messages,
    QSharedPointer<Component> component,
    QSharedPointer<View> activeView) :
    messages_(messages),
    component_(component),
    activeView_(activeView),
    parameters_(new QList<QSharedPointer<Parameter> >()),
    moduleParameters_(new QList<QSharedPointer<Parameter> >()),
    metaParameters_(new QList<QSharedPointer<Parameter> >()),
    ports_(new QMap<QString,QSharedPointer<MetaPort> >()),
    fileSets_(new QList<QSharedPointer<FileSet> >()),
    moduleName_(),
    activeInstantiation_(),
    remapStates_(new QList<QSharedPointer<FormattedRemapState> >())
{
    // Try to find a component instantiation for the view.
    if (activeView_)
    {
        activeInstantiation_ = Search::findByName(activeView_->getComponentInstantiationRef(),
            *component_->getComponentInstantiations());

        if (activeInstantiation_)
        {
            // If there is a named component instantiation, its module name shall be used.
            moduleName_ = activeInstantiation_->getModuleName();
            parsesFileSets();
        }
    }

    if (moduleName_.isEmpty())
    {
        // If no module name is set, take the name from the VLNV of the component.
        moduleName_ = component_->getVlnv().getName();
    }

    // Must parse the parameters before can use them!
    parseParameters();
}

//-----------------------------------------------------------------------------
// Function:  MetaComponent::formatComponent()
//-----------------------------------------------------------------------------
void MetaComponent::formatComponent()
{
    // Initialize the parameter finders.
    QSharedPointer<MultipleParameterFinder> parameterFinder(new MultipleParameterFinder());
    QSharedPointer<ComponentParameterFinder> componentFinder(new ComponentParameterFinder(component_));
    QSharedPointer<ListParameterFinder> moduleFinder(new ListParameterFinder());
    moduleFinder->setParameterList(getModuleParameters());
    parameterFinder->addFinder(componentFinder);
    parameterFinder->addFinder(moduleFinder);

    //! The formatter for expressions.
    ExpressionFormatter formatter(parameterFinder);

    parameters_->clear();
    moduleParameters_->clear();
    metaParameters_->clear();
    parseParameters();
    formatParameters(formatter);
    formatPorts(formatter);
    parseRemapStates(formatter);

    parseMetaParameters();
}

//-----------------------------------------------------------------------------
// Function: MetaComponent::parseMetaParameters()
//-----------------------------------------------------------------------------
void MetaComponent::parseMetaParameters()
{
    for (QSharedPointer<Parameter> original : *getParameters())
    {
        metaParameters_->append(original);
    }

    for (QSharedPointer<Parameter> original : *getModuleParameters())
    {
        Q_ASSERT(original);

        auto i = std::find_if(metaParameters_->begin(), metaParameters_->end(),
            [&original](QSharedPointer<Parameter> parameter)
        { return original->getValue().contains(parameter->name()); });

        if (i != metaParameters_->end())
        {
            if (original->getValue().compare((*i)->name()) == 0)
            {
                original->setValue((*i)->getValue());
            }

            metaParameters_->erase(i);
        }

        metaParameters_->append(original);
    }
}

//-----------------------------------------------------------------------------
// Function: MetaComponent::parseParameters()
//-----------------------------------------------------------------------------
void MetaComponent::parseParameters()
{
    // Copy all the component parameters for the original parameters.
    for (QSharedPointer<Parameter> parameterOrig : *component_->getParameters())
    {       
        parameters_->append(QSharedPointer<Parameter>(new Parameter(*parameterOrig)));
    }

    // If there is an active component instantiation, take its module parameters as well.
    if (activeInstantiation_)
    {
        for (QSharedPointer<ModuleParameter> parameterOrig : *activeInstantiation_->getModuleParameters())
        {
            moduleParameters_->append(QSharedPointer<ModuleParameter>(new ModuleParameter(*parameterOrig)));
        }
    }
}

//-----------------------------------------------------------------------------
// Function: MetaComponent::formatParameters()
//-----------------------------------------------------------------------------
void MetaComponent::formatParameters(ExpressionFormatter const& formatter)
{
	sortParameters(parameters_);
    for (QSharedPointer<Parameter> parameter : *parameters_)
    {
        parameter->setValue(formatter.formatReferringExpression(parameter->getValue()));
    }

    sortParameters(moduleParameters_);
    for (QSharedPointer<Parameter> moduleParameter : *moduleParameters_)
    {
        moduleParameter->setValue(formatter.formatReferringExpression(moduleParameter->getValue()));
    }
}

//-----------------------------------------------------------------------------
// Function: MetaComponent::sortParameters()
//-----------------------------------------------------------------------------
void MetaComponent::sortParameters(QSharedPointer<QList<QSharedPointer<Parameter> > > sortParameters)
{
    std::sort(sortParameters->begin(), sortParameters->end(),
        [](QSharedPointer<Parameter>& currentParameter, QSharedPointer<Parameter>& nextParameter)
        {
            if (nextParameter->getValue().contains(currentParameter->getValueId()))
            {
                return true;
            }

            if (currentParameter->getValue().contains(nextParameter->getValueId()))
            {
                return false;
            }
        
            return currentParameter->name() < nextParameter->name();
        });
}

//-----------------------------------------------------------------------------
// Function: MetaComponent::parsesFileSets()
//-----------------------------------------------------------------------------
void MetaComponent::parsesFileSets()
{
    for (auto const& fileSetRef : *activeInstantiation_->getFileSetReferences())
    {
        QSharedPointer<FileSet> fileSet = component_->getFileSet(fileSetRef->getReference());
        if (fileSet)
        {
            fileSets_->append(fileSet);
        }
    }
}

//-----------------------------------------------------------------------------
// Function: MetaComponent::formatPorts()
//-----------------------------------------------------------------------------
void MetaComponent::formatPorts(ExpressionFormatter const& formatter)
{
    for (QSharedPointer<Port> cport : *component_->getPorts())
    {
        QSharedPointer<MetaPort> mPort(new MetaPort);
        mPort->port_ = cport;
        mPort->arrayBounds_.first = "";
        mPort->arrayBounds_.second = "";
        mPort->width_ = "";
        mPort->vectorBounds_.first = formatter.formatReferringExpression(cport->getLeftBound());
        mPort->vectorBounds_.second = formatter.formatReferringExpression(cport->getRightBound());
        mPort->isWire_ = false;
        mPort->isTransactional_ = false;

        if (cport->getWire())
        {
            mPort->isWire_ = true;
            mPort->arrayBounds_.first = formatter.formatReferringExpression(cport->getArrayLeft());
            mPort->arrayBounds_.second = formatter.formatReferringExpression(cport->getArrayRight());
        }
        else if (cport->getTransactional())
        {
            mPort->isTransactional_ = true;
            mPort->width_ = formatter.formatReferringExpression(cport->getTransactional()->getBusWidth());
        }

        ports_->insert(cport->name(), mPort);
    }
}

//-----------------------------------------------------------------------------
// Function: MetaComponent::parseRemapStates
//-----------------------------------------------------------------------------
void MetaComponent::parseRemapStates(ExpressionFormatter const& formatter)
{
    for (QSharedPointer<RemapState> currentState : *component_->getRemapStates())
    {
        QSharedPointer<FormattedRemapState> remapState(new FormattedRemapState);
        remapState->state_ = currentState;
        remapStates_->append(remapState);

        // Each port referred by the state must be listed.
        for (QSharedPointer<RemapPort> remapPort : *currentState->getRemapPorts())
        {
            // Pick the port name, and the value needed for it to remap state become effective.
            QSharedPointer<QPair<QSharedPointer<Port>,QString> > parsedPort
                (new QPair<QSharedPointer<Port>, QString>);
            parsedPort->first = component_->getPort(remapPort->getPortNameRef());
            parsedPort->second = formatter.formatReferringExpression(remapPort->getValue());

            remapState->ports_.append(parsedPort);
        }
    }
}
