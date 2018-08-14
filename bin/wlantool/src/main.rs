// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![feature(futures_api, arbitrary_self_types, pin)]
#![deny(warnings)]

use failure::{format_err, Error, Fail, ResultExt};
use fidl::endpoints2;
use fidl_fuchsia_wlan_device_service::{self as wlan_service, DeviceServiceMarker,
                                       DeviceServiceProxy};
use fidl_fuchsia_wlan_sme::{self as fidl_sme, ConnectResultCode, ConnectTransactionEvent,
                            ScanTransactionEvent};
use fuchsia_app::client::connect_to_service;
use fuchsia_async::temp::TempFutureExt;
use fuchsia_async::{self as fasync, unsafe_many_futures};
use fuchsia_zircon as zx;
use futures::prelude::*;
use std::fmt;
use structopt::StructOpt;

mod opts;
use crate::opts::*;

type WlanSvc = DeviceServiceProxy;

fn main() -> Result<(), Error> {
    let opt = Opt::from_args();
    println!("{:?}", opt);

    let mut exec = fasync::Executor::new().context("error creating event loop")?;
    let wlan_svc =
        connect_to_service::<DeviceServiceMarker>().context("failed to connect to device service")?;

    match opt {
        Opt::Phy(cmd) => exec.run_singlethreaded(do_phy(cmd, wlan_svc)),
        Opt::Iface(cmd) => exec.run_singlethreaded(do_iface(cmd, wlan_svc)),
        Opt::Client(cmd) => exec.run_singlethreaded(do_client(cmd, wlan_svc)),
    }
}

fn do_phy(cmd: opts::PhyCmd, wlan_svc: WlanSvc) -> impl Future<Output = Result<(), Error>> {
    match cmd {
        opts::PhyCmd::List => {
            // TODO(tkilbourn): add timeouts to prevent hanging commands
            wlan_svc
                .list_phys()
                .map_err(|e| e.context("error getting response").into())
                .map_ok(|response| {
                    println!("response: {:?}", response);
                }).left_future()
        },
        opts::PhyCmd::Query { phy_id } => {
            let mut req = wlan_service::QueryPhyRequest { phy_id };
            wlan_svc
                .query_phy(&mut req)
                .map_err(|e| e.context("error querying phy").into())
                .map_ok(|response| {
                    println!("response: {:?}", response);
                }).right_future()
        }
    }
}

fn do_iface(cmd: opts::IfaceCmd, wlan_svc: WlanSvc) -> impl Future<Output = Result<(), Error>> {
    unsafe_many_futures!(IfaceFut, [ New, Delete, List ]);
    match cmd {
        opts::IfaceCmd::New { phy_id, role } => IfaceFut::New({
            let mut req = wlan_service::CreateIfaceRequest {
                phy_id: phy_id,
                role: role.into(),
            };

            wlan_svc
                .create_iface(&mut req)
                .map_err(|e| e.context("error getting response").into())
                .map_ok(|response| {
                    println!("response: {:?}", response);
                })
        }),
        opts::IfaceCmd::Delete { phy_id, iface_id } => IfaceFut::Delete({
            let mut req = wlan_service::DestroyIfaceRequest {
                phy_id: phy_id,
                iface_id: iface_id,
            };

            wlan_svc
                .destroy_iface(&mut req)
                .map_ok(move |status| match zx::Status::ok(status) {
                    Ok(()) => println!("destroyed iface {:?}", iface_id),
                    Err(s) => println!("error destroying iface: {:?}", s),
                })
                .map_err(|e| e.context("error destroying iface").into())
        }),
        opts::IfaceCmd::List => IfaceFut::List({
            wlan_svc.list_ifaces()
                .map_err(|e| e.context("error getting response").into())
                .map_ok(|response| {
                    println!("response: {:?}", response);
                })
        }),
    }
}

fn do_client(cmd: opts::ClientCmd, wlan_svc: WlanSvc) -> impl Future<Output = Result<(), Error>> {
    unsafe_many_futures!(ClientFut, [ Scan, Connect, Status ]);
    match cmd {
        opts::ClientCmd::Scan { iface_id } => ClientFut::Scan(get_client_sme(wlan_svc, iface_id)
            .err_into::<Error>()
            .and_then(|sme| {
                let (local, remote) = match endpoints2::create_endpoints() {
                    Ok(x) => x,
                    Err(e) => return future::ready(Err(e.into())),
                };
                let mut req = fidl_sme::ScanRequest {
                    timeout: 10
                };
                future::ready(
                    sme.scan(&mut req, remote)
                        .map(move |()| local)
                        .map_err(|e| e.context("error sending scan request").into()))
            })
            .and_then(handle_scan_transaction)),
        opts::ClientCmd::Connect { iface_id, ssid, password } =>
            ClientFut::Connect(get_client_sme(wlan_svc, iface_id)
            .and_then(move |sme| {
                let (local, remote) = match endpoints2::create_endpoints() {
                    Ok(x) => x,
                    Err(e) => return future::ready(Err(e.into())),
                };
                let mut req = fidl_sme::ConnectRequest {
                    ssid: ssid.as_bytes().to_vec(),
                    password: password.as_bytes().to_vec(),
                };
                future::ready(sme.connect(&mut req, Some(remote))
                    .map(move |()| local)
                    .map_err(|e| e.context("error sending connect request").into()))
            })
            .and_then(handle_connect_transaction)
            .err_into()),
        opts::ClientCmd::Status { iface_id } => ClientFut::Status(get_client_sme(wlan_svc, iface_id)
            .and_then(|sme| sme.status().err_into())
            .map_ok(|st| {
                match st.connected_to {
                    Some(bss) => {
                        println!("Connected to '{}' (bssid {})",
                                 String::from_utf8_lossy(&bss.ssid), Bssid(bss.bssid));
                    },
                    None => println!("Not connected to a network"),
                }
                if !st.connecting_to_ssid.is_empty() {
                    println!("Connecting to '{}'", String::from_utf8_lossy(&st.connecting_to_ssid));
                }
            }))
    }
}

struct Bssid([u8; 6]);

impl fmt::Display for Bssid {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
               self.0[0], self.0[1], self.0[2], self.0[3], self.0[4], self.0[5])
    }
}

fn handle_scan_transaction(scan_txn: fidl_sme::ScanTransactionProxy)
    -> impl Future<Output = Result<(), Error>>
{
    let mut printed_header = false;
    scan_txn.take_event_stream()
        .map_ok(move |e| {
            match e {
                ScanTransactionEvent::OnResult { aps } => {
                    if !printed_header {
                        print_scan_header();
                        printed_header = true;
                    }
                    for ap in aps {
                        print_scan_result(ap);
                    }
                    false
                },
                ScanTransactionEvent::OnFinished { } => true,
                ScanTransactionEvent::OnError { error } => {
                    eprintln!("Error: {}", error.message);
                    true
                },
            }
        })
        .try_fold(false, |_prev, done| future::ready(Ok(done)))
        .err_into::<Error>()
        .and_then(|done| {
            future::ready(if done {
                Ok(())
            } else {
                Err(format_err!("Failed to fetch all results before the channel was closed"))
            })
        })
}

fn print_scan_header() {
    println!("BSSID             dBm  Channel Protected SSID");
}

fn print_scan_result(ess: fidl_sme::EssInfo) {
    println!("{} {:4} {:7} {:9} {}",
        Bssid(ess.best_bss.bssid),
        ess.best_bss.rx_dbm,
        ess.best_bss.channel,
        if ess.best_bss.protected { "Y" } else { "N" },
        String::from_utf8_lossy(&ess.best_bss.ssid));
}

fn handle_connect_transaction(connect_txn: fidl_sme::ConnectTransactionProxy)
    -> impl Future<Output = Result<(), Error>>
{
    connect_txn.take_event_stream()
        .map_ok(|e| {
            match e {
                ConnectTransactionEvent::OnFinished { code } => {
                    match code {
                        ConnectResultCode::Success => println!("Connected successfully"),
                        ConnectResultCode::Canceled =>
                            eprintln!("Connecting was canceled or superseded by another command"),
                        ConnectResultCode::Failed => eprintln!("Failed to connect to network"),
                        ConnectResultCode::BadCredentials =>
                            eprintln!("Failed to connect to network; bad credentials"),
                    }
                    true
                },
            }
        })
        .try_fold(false, |_prev, done| future::ready(Ok(done)))
        .err_into::<Error>()
        .and_then(|done| {
            future::ready(if done {
                Ok(())
            } else {
                Err(format_err!(
                    "Failed to receiver a connect result before the channel was closed"))
            })
        })
}

fn get_client_sme(wlan_svc: WlanSvc, iface_id: u16)
    -> impl Future<Output = Result<fidl_sme::ClientSmeProxy, Error>>
{
    future::ready(endpoints2::create_endpoints())
        .err_into()
        .and_then(move |(proxy, remote)| {
            wlan_svc.get_client_sme(iface_id, remote)
                .map_err(|e| e.context("error sending GetClientSme request").into())
                .and_then(move |status| {
                    future::ready(if status == zx::sys::ZX_OK {
                        Ok(proxy)
                    } else {
                        Err(format_err!("Invalid interface id {}", iface_id))
                    })
                })
        })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn format_bssid() {
        assert_eq!("01:02:03:ab:cd:ef",
               format!("{}", Bssid([ 0x01, 0x02, 0x03, 0xab, 0xcd, 0xef])));
    }
}
