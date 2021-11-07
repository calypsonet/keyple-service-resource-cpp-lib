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

#include "CardProfileManagerAdapter.h"

#include <algorithm>
#include <random>

/* Keyple Core Util */
#include "InterruptedException.h"
#include "System.h"

/* Keyple Service Resource */
#include "ReaderManagerAdapter.h"

/* Keyple Core Service */
#include "KeyplePluginException.h"

namespace keyple {
namespace core {
namespace service {
namespace resource {

using namespace keyple::core::service;
using namespace keyple::core::util::cpp;
using namespace keyple::core::util::cpp::exception;

CardProfileManagerAdapter::CardProfileManagerAdapter(
  std::shared_ptr<CardResourceProfileConfigurator> cardProfile,
  std::shared_ptr<CardResourceServiceConfiguratorAdapter> globalConfiguration)
: mCardProfile(cardProfile),
  mGlobalConfiguration(globalConfiguration),
  mService(CardResourceServiceAdapter::getInstance())
{
    /* Prepare filter on reader name if requested */
    if (cardProfile->getReaderNameRegex() != "") {
        mReaderNameRegexPattern = Pattern::compile(cardProfile->getReaderNameRegex());
    } else {
        mReaderNameRegexPattern = nullptr;
    }

    /* Initialize all available card resources */
    if (!cardProfile->getPlugins().empty()) {
        initializeCardResourcesUsingProfilePlugins();
    } else {
        initializeCardResourcesUsingDefaultPlugins();
    }
}

void CardProfileManagerAdapter::removeCardResource(std::shared_ptr<CardResource> cardResource) 
{
    bool isRemoved = mCardResources.erase(cardResource);
    if (isRemoved) {
        mLogger->debug("Remove % from card resource profile '%'\n",
                       CardResourceServiceAdapter::getCardResourceInfo(cardResource),
                       mCardProfile->getProfileName());
    }
}

void CardProfileManagerAdapter::onReaderConnected(
    std::shared_ptr<ReaderManagerAdapter> readerManager)
{
    if (!mCardProfile->getPlugins().empty()) {
        for (const auto& profilePlugin : mCardProfile->getPlugins()) {
            if (profilePlugin == readerManager->getPlugin()) {
                initializeCardResource(readerManager);
                break;
            }
        }
    } else {
        initializeCardResource(readerManager);
    }
}

void CardProfileManagerAdapter::onCardInserted(std::shared_ptr<ReaderManagerAdapter> readerManager)
{
    onReaderConnected(readerManager);
}

std::shared_ptr<CardResource> CardProfileManagerAdapter::getCardResource()
{
    std::shared_ptr<CardResource> cardResource = nullptr;
    long maxTime = System::currentTimeMillis() + mGlobalConfiguration->getTimeoutMillis();
    
    do {
        if (!mPlugins.empty()) {
            if (!mPoolPlugins.empty()) {
                cardResource = getRegularOrPoolCardResource();
            } else {
                cardResource = getRegularCardResource();
            }
        } else {
            cardResource = getPoolCardResource();
        }
    
        pauseIfNeeded(cardResource);
    
    } while (cardResource == nullptr &&
             mGlobalConfiguration->isBlockingAllocationMode() &&
             System::currentTimeMillis() <= maxTime);

    return cardResource;
}

void CardProfileManagerAdapter::initializeCardResourcesUsingProfilePlugins()
{
    for (const auto& plugin : mCardProfile->getPlugins()) {
        const auto poolPlugin = std::dynamic_pointer_cast<PoolPlugin>(plugin);
        if (poolPlugin) {
            mPoolPlugins.push_back(poolPlugin);
        } else {
            mPlugins.push_back(plugin);
            initializeCardResources(plugin);
        }
    }
}

void CardProfileManagerAdapter::initializeCardResourcesUsingDefaultPlugins()
{
    const auto& poolPlugins = mGlobalConfiguration->getPoolPlugins();
    mPoolPlugins.insert(std::end(mPoolPlugins), std::begin(poolPlugins), std::end(poolPlugins));
    
    for (const auto& plugin : mGlobalConfiguration->getPlugins()) {
        mPlugins.push_back(plugin);
        initializeCardResources(plugin);
    }
}

void CardProfileManagerAdapter::initializeCardResources(std::shared_ptr<Plugin> plugin)
{
    for (const auto& reader : plugin->getReaders()) {
        std::shared_ptr<ReaderManagerAdapter> readerManager = mService->getReaderManager(reader);
        initializeCardResource(readerManager);
    }
}

void CardProfileManagerAdapter::initializeCardResource(
    std::shared_ptr<ReaderManagerAdapter> readerManager)
{
    if (isReaderAccepted(readerManager->getReader())) {

        readerManager->activate();

        std::shared_ptr<CardResource> cardResource =
            readerManager->matches(mCardProfile->getCardResourceProfileExtensionSpi());

        /*
         * The returned card resource may already be present in the current list if the service
         * starts with an observable reader in which a card has been inserted.
         */
        if (cardResource != nullptr) {
            if (!mCardResources.contains(cardResource)) {
                mCardResources.push_back(cardResource);
                mLogger->debug("Add % to card resource profile '%'\n",
                               CardResourceServiceAdapter::getCardResourceInfo(cardResource),
                               mCardProfile->getProfileName());
            } else {
                mLogger->debug("% already present in card resource profile '%'\n",
                                CardResourceServiceAdapter::getCardResourceInfo(cardResource),
                                mCardProfile->getProfileName());
            }
        }
    }
}

bool CardProfileManagerAdapter::isReaderAccepted(std::shared_ptr<CardReader> reader)
{
    return mReaderNameRegexPattern == nullptr || 
           mReaderNameRegexPattern->matcher(reader->getName())->matches();
}

void CardProfileManagerAdapter::pauseIfNeeded(std::shared_ptr<CardResource> cardResource)
{
    if (cardResource == nullptr && mGlobalConfiguration->isBlockingAllocationMode()) {
        try {
            Thread::sleep(mGlobalConfiguration->getCycleDurationMillis());
        } catch (const InterruptedException& e) {
            mLogger->error("Unexpected sleep interruption", e);
            // FIXME: Thread::currentThread().interrupt();
        }
    }
}

std::shared_ptr<CardResource> CardProfileManagerAdapter::getRegularOrPoolCardResource()
{
    std::shared_ptr<CardResource> cardResource = nullptr;

    if (mGlobalConfiguration->isUsePoolFirst()) {
        cardResource = getPoolCardResource();
        if (cardResource == nullptr) {
            cardResource = getRegularCardResource();
        }
    } else {
        cardResource = getRegularCardResource();
        if (cardResource == nullptr) {
            cardResource = getPoolCardResource();
        }
    }

    return cardResource;
}

std::shared_ptr<CardResource> CardProfileManagerAdapter::getRegularCardResource()
{
    std::shared_ptr<CardResource> result = nullptr;
    std::vector<std::shared_ptr<CardResource>> unusableCardResources;

    for (const std::shared_ptr<CardResource>& cardResource : mCardResources) {
        std::shared_ptr<CardReader> reader = cardResource->getReader();
        //FIXME synchronized (reader) {
            std::shared_ptr<ReaderManagerAdapter> readerManager = 
                mService->getReaderManager(reader);
            if (readerManager != nullptr) {
                try {
                    if (readerManager->lock(
                            cardResource, 
                            mCardProfile->getCardResourceProfileExtensionSpi())) {
                        int cardResourceIndex = Arrays::indexOf(cardResources, cardResource);
                        updateCardResourcesOrder(cardResourceIndex);
                        result = cardResource;
                        break;
                    }
                } catch (const IllegalStateException& e) {
                    (void)e;
                    unusableCardResources.push_back(cardResource);
                }
            } else {
                unusableCardResources.push_back(cardResource);
            }
        //}
    }

    /* Remove unusable card resources identified */
    for (const auto& cardResource : unusableCardResources) {
        mService->removeCardResource(cardResource);
    }

    return result;
}

void CardProfileManagerAdapter::updateCardResourcesOrder(const int cardResourceIndex)
{
    if (mGlobalConfiguration->getAllocationStrategy() == AllocationStrategy::CYCLIC) {
        std::rotate(cardResources.begin(), 
                    cardResources.begin() - cardResourceIndex - 1,
                    cardResources.end());
    } else if (mGlobalConfiguration->getAllocationStrategy() == AllocationStrategy::RANDOM) {
        auto rng = std::default_random_engine {};
        std::shuffle(std::begin(cardResources), std::end(cardResources), rng);
    }
}

std::shared_ptr<CardResource> CardProfileManagerAdapter::getPoolCardResource()
{
    for (const std::shared_ptr<PoolPlugin>& poolPlugin : mPoolPlugins) {
        try {
            std::shared_ptr<Reader> reader =
                poolPlugin->allocateReader(mCardProfile->getReaderGroupReference());
            if (reader != nullptr) {
                std::shared_ptr<SmartCard> smartCard = 
                    mCardProfile->getCardResourceProfileExtensionSpi()
                                ->matches(reader, 
                                          SmartCardServiceProvider::getService()
                                              ->createCardSelectionManager());
                if (smartCard != nullptr) {
                    auto cardResource = std::make_shared<CardResource>(reader, smartCard);
                    mService->registerPoolCardResource(cardResource, poolPlugin);
                    return cardResource;
                }
            }
        } catch (const KeyplePluginException& e) {
            (void)e;
            /* Continue */
        }
    }

    return nullptr;
}

}
}
}
}