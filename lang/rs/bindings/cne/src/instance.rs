/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020-2022 Intel Corporation.
 */

use std::mem::MaybeUninit;
use std::sync::Once;
use std::sync::RwLock;
use std::sync::{Mutex, MutexGuard};
use std::sync::{RwLockReadGuard, RwLockWriteGuard};

use cne_sys::bindings::{cne_id, cne_register, cne_unregister, pktmbuf_info_t};

use super::config::*;
use super::error::*;
use super::port::*;
use super::util::*;

pub struct CneInstance {
    // Use RwLock to allow multiple concurrent reader threads
    // and single writer to access CNE Instance.
    inner: RwLock<CneInstanceInner>,
    lock: Mutex<()>,
}

struct CneInstanceInner {
    cfg: Option<Config>,
    tid: i32,
}

// CNE Singleton instance.
impl CneInstance {
    pub fn get_instance() -> &'static CneInstance {
        // Create an uninitialized static.
        static mut CNEINSTANCE: MaybeUninit<CneInstance> = MaybeUninit::uninit();
        static ONCE: Once = Once::new();

        unsafe {
            ONCE.call_once(|| {
                let cne_inner = CneInstanceInner { cfg: None, tid: -1 };
                // Create instance.
                let singleton = CneInstance {
                    inner: RwLock::new(cne_inner),
                    lock: Mutex::new(()),
                };
                // Store instance to the static variable.
                CNEINSTANCE.write(singleton);
            });

            // Return shared reference to the cne instance, safe for concurrent access.
            CNEINSTANCE.assume_init_ref()
        }
    }
}

// CneInstance public functions.
impl CneInstance {
    pub fn configure(&self, jsonc_file: &str) -> Result<(), CneError> {
        let mut cne = self.write()?;
        if cne.cfg.is_some() {
            // CNE is already configured. Using existing config.
            return Ok(());
        }

        // Register calling thread with CNE.
        cne.tid = self.register_thread("configure")?;

        // Load config.
        let mut cfg = Config::load_config(jsonc_file)?;

        // Setup.
        cfg.setup()?;
        cne.cfg = Some(cfg);

        Ok(())
    }

    pub fn cleanup(&self) -> Result<(), CneError> {
        let mut cne = self.write()?;
        if let Some(cfg) = &mut cne.cfg {
            cfg.cleanup()?;
        }
        cne.cfg = None;

        // Unregister thread.
        if cne.tid >= 0 {
            self.unregister_thread(cne.tid)?;
        }
        cne.tid = -1;

        Ok(())
    }

    pub fn register_thread(&self, s: &str) -> Result<i32, CneError> {
        let _lock = self.lock()?;

        let mut tid = unsafe { cne_id() };
        if tid < 0 {
            // Register thread with CNE.
            tid = unsafe { cne_register(get_cstring_from_str(s).as_ptr()) };
        }
        if tid < 0 {
            Err(CneError::RegisterError(
                "Error registering thread with CNE".to_string(),
            ))
        } else {
            Ok(tid)
        }
    }

    pub fn unregister_thread(&self, tid: i32) -> Result<(), CneError> {
        let _lock = self.lock()?;

        let ret = unsafe { cne_unregister(tid) };
        if ret < 0 {
            Err(CneError::RegisterError(
                "Error unregistering thread with CNE".to_string(),
            ))
        } else {
            Ok(())
        }
    }

    pub fn get_port(&self, port_index: u16) -> Result<Port, CneError> {
        let cne = self.read()?;

        if let Some(cfg) = &cne.cfg {
            cfg.get_port_by_index(port_index)
        } else {
            Err(CneError::ConfigError(
                "Cannot find port. CNE is not configured".to_string(),
            ))
        }
    }

    pub fn get_port_by_name(&self, port_name: &str) -> Result<Port, CneError> {
        let cne = self.read()?;

        if let Some(cfg) = &cne.cfg {
            cfg.get_port_by_name(port_name)
        } else {
            Err(CneError::ConfigError(
                "Cannot find port. CNE is not configured".to_string(),
            ))
        }
    }

    pub(crate) fn get_port_pktmbuf_pool(
        &self,
        port_index: u16,
    ) -> Result<*mut pktmbuf_info_t, CneError> {
        let cne = self.read()?;

        if let Some(cfg) = &cne.cfg {
            cfg.get_port_pktmbuf_pool(port_index)
        } else {
            Err(CneError::ConfigError(
                "Cannot find port. CNE is not configured".to_string(),
            ))
        }
    }
}

// CneInstance private functions.
impl CneInstance {
    fn read(&self) -> Result<RwLockReadGuard<CneInstanceInner>, CneError> {
        self.inner
            .read()
            .map_err(|e| CneError::PortError(e.to_string()))
    }

    fn write(&self) -> Result<RwLockWriteGuard<CneInstanceInner>, CneError> {
        self.inner
            .write()
            .map_err(|e| CneError::PortError(e.to_string()))
    }

    fn lock(&self) -> Result<MutexGuard<()>, CneError> {
        self.lock
            .lock()
            .map_err(|e| CneError::PortError(e.to_string()))
    }
}
