/******************************************************************************
 * Copyright © 2013-2019 The Komodo Platform Developers.                      *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * Komodo Platform software, including this file may be copied, modified,     *
 * propagated or distributed except according to the terms contained in the   *
 * LICENSE file                                                               *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include <QDebug>
#include <QTimer>

//! Project Headers
#include "atomic.dex.app.hpp"
#include "atomic.dex.mm2.hpp"
#include "atomic.dex.provider.coinpaprika.hpp"
#include "atomic.dex.qt.bindings.hpp"
#include "atomic.dex.security.hpp"

namespace atomic_dex
{
    QObjectList
    atomic_dex::application::get_enabled_coins() const noexcept
    {
        return m_enabled_coins;
    }

    bool
    atomic_dex::application::create(const QString& password, const QString& seed)
    {
        std::error_code ec;
        auto            key = atomic_dex::derive_password(password.toStdString(), ec);
        if (ec)
        {
            DLOG_F(WARNING, "{}", ec.message());
            if (ec == dextop_error::derive_password_failed)
            {
                return false;
            }
        }
        else
        {
            const std::filesystem::path seed_path = ag::core::assets_real_path() / "config/encrypted.seed";
            // Encrypt seed
            atomic_dex::encrypt(seed_path, seed.toStdString().data(), key.data());
            // sodium_memzero(&seed, seed.size());
            sodium_memzero(key.data(), key.size());

            return true;
        }
        return false;
    }

    bool
    atomic_dex::application::login(const QString& password)
    {
        std::error_code ec;
        auto            key = atomic_dex::derive_password(password.toStdString(), ec);
        if (ec)
        {
            DLOG_F(WARNING, "{}", ec.message());
            if (ec == dextop_error::derive_password_failed)
            {
                return false;
            }
        }
        else
        {
            const std::filesystem::path seed_path = ag::core::assets_real_path() / "config/encrypted.seed";
            auto                        seed      = atomic_dex::decrypt(seed_path, key.data(), ec);
            if (ec == dextop_error::corrupted_file_or_wrong_password)
            {
                LOG_F(WARNING, "{}", ec.message());
                return false;
            }
            else
            {
                get_mm2().spawn_mm2_instance(seed);
                return true;
            }
        }
        return false;
    }

    bool
    atomic_dex::application::first_run()
    {
        return not fs::exists(ag::core::assets_real_path() / "config/encrypted.seed");
    }

    void
    application::launch()
    {
        auto timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &application::tick);
        timer->start(8);
    }

    QString
    atomic_dex::application::get_mnemonic()
    {
        bc::data_chunk my_entropy_256(32); // 32 bytes = 256 bits

        bc::pseudo_random_fill(my_entropy_256);

        // Instantiate mnemonic word_list
        bc::wallet::word_list words = bc::wallet::create_mnemonic(my_entropy_256);
        return QString::fromStdString(bc::join(words));
    }

    void
    application::tick()
    {
        this->process_one_frame();
        auto& mm2 = get_mm2();
        if (mm2.is_mm2_running())
        {
            //! Enabled coins stuff
            {
                auto coins = mm2.get_enabled_coins();
                this->m_enabled_coins.clear();
                this->m_enabled_coins = to_qt_binding(std::move(coins), this);
                emit enabled_coins_changed();
            }
        }
    }

    mm2&
    application::get_mm2() noexcept
    {
        return this->system_manager_.get_system<mm2>();
    }

    coinpaprika_provider&
    application::get_paprika() noexcept
    {
        return this->system_manager_.get_system<coinpaprika_provider>();
    }

    entt::dispatcher&
    application::get_dispatcher() noexcept
    {
        return this->dispatcher_;
    }

    application::application(QObject* pParent) noexcept : QObject(pParent)
    {
        //! MM2 system need to be created before the GUI and give the instance to the gui
        auto& mm2_system = system_manager_.create_system<mm2>();
        system_manager_.create_system<coinpaprika_provider>(mm2_system);
    }
} // namespace atomic_dex