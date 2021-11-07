/**************************************************************************************************
 * Copyright (c) 2021 Calypso Networks Association https://calypsonet.org/                        *
 *                                                                                                *
 * See the NOTICE file(s) distributed with this work for additional information regarding         *
 * copyright ownership.                                                                           *
 *                                                                                                *
 * This program and the accompanying materials are made available under the terms of the Eclipse  *
 * Public License 2.0 which is available at http://www.eclipse.org/legal/epl-2.0                  *
 *                                                                                                *
 * SPDX-License-Identifier: EPL-2.0                                                               *
 **************************************************************************************************/

#include "CardResourceProfileConfigurator.h"

namespace keyple {
namespace core {
namespace service {
namespace resource {

/* CARD RESOURCE PROFILE CONFIGURATOR ----------------------------------------------------------- */

CardResourceProfileConfigurator::CardResourceProfileConfigurator(Builder* builder)
: mProfileName(builder->profileName),
  mCardResourceProfileExtensionSpi(builder->cardResourceProfileExtensionSpi),
  mPlugins(builder->mPlugins),
  mReaderNameRegex(builder->readerNameRegex),
  mReaderGroupReference(builder->readerGroupReference) {}

const std::string& CardResourceProfileConfigurator::getProfileName() const
{
    return mProfileName;
}

std::shared_ptr<CardResourceProfileExtension>
    CardResourceProfileConfigurator::getCardResourceProfileExtensionSpi() const
{
    return mCardResourceProfileExtensionSpi;
}

const std::vector<std::shared_ptr<Plugin>> CardResourceProfileConfigurator::getPlugins() const
{
    return mPlugins;
}

const std::string& CardResourceProfileConfigurator::getReaderNameRegex() const
{
    return mReaderNameRegex;
}

const std::string& CardResourceProfileConfigurator::getReaderGroupReference() const
{
    return mReaderGroupReference;
}

Builder* CardResourceProfileConfigurator::builder(
    const std::string& profileName, 
    std::shared_ptr<CardResourceProfileExtension> cardResourceProfileExtension)
{    
    return new Builder(profileName, cardResourceProfileExtension);
}

/* BUILDER -------------------------------------------------------------------------------------- */

CardResourceProfileConfigurator::Builder::Builder(
  const std::string& profileName, 
  std::shared_ptr<CardResourceProfileExtension> cardResourceProfileExtension)
: mProfileName(profileName), mCardResourceProfileExtensionSpi(cardResourceProfileExtension)
{
    Assert::getInstance().notNull(cardResourceProfileExtension, "cardResourceProfileExtension");
    
    const auto cardResource
    if (!(cardResourceProfileExtension instanceof CardResourceProfileExtension)) {
        throw new IllegalArgumentException(
            "The provided card profile extension does not implement the right internal SPI.");
    }
    
    this.profileName = profileName;
    this.cardResourceProfileExtensionSpi =
        (CardResourceProfileExtension) cardResourceProfileExtension;
    this.plugins = new ArrayList<Plugin>(1);
    this.readerNameRegex = null;
    this.readerGroupReference = null;
}

}
}
}
}
