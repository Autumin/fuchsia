// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::apply::Initiator;
use crate::channel::{CurrentChannelManager, TargetChannelManager};
use crate::connect::ServiceConnector;
use crate::update_manager::{
    CurrentChannelUpdater, RealUpdateApplier, RealUpdateChecker, TargetChannelUpdater,
    UpdateApplier, UpdateChecker, UpdateManager,
};
use anyhow::{format_err, Context as _, Error};
use event_queue::{ClosedClient, Notify};
use fidl::endpoints::ClientEnd;
use fidl_fuchsia_update::{
    CheckNotStartedReason, CheckOptions, ManagerCheckNowResponder, ManagerRequest,
    ManagerRequestStream, MonitorMarker,
};
use fidl_fuchsia_update_ext::State;
use futures::{future::BoxFuture, prelude::*};
use std::sync::Arc;

pub type RealTargetChannelUpdater = TargetChannelManager<ServiceConnector>;
pub type RealCurrentChannelUpdater = CurrentChannelManager;
pub type RealUpdateService = UpdateService<
    RealTargetChannelUpdater,
    RealCurrentChannelUpdater,
    RealUpdateChecker,
    RealUpdateApplier,
>;
pub type RealUpdateManager = UpdateManager<
    RealTargetChannelUpdater,
    RealCurrentChannelUpdater,
    RealUpdateChecker,
    RealUpdateApplier,
    RealStateNotifier,
>;

pub struct UpdateService<T, Ch, C, A>
where
    T: TargetChannelUpdater,
    Ch: CurrentChannelUpdater,
    C: UpdateChecker,
    A: UpdateApplier,
{
    update_manager: Arc<UpdateManager<T, Ch, C, A, RealStateNotifier>>,
}

impl<T, Ch, C, A> Clone for UpdateService<T, Ch, C, A>
where
    T: TargetChannelUpdater,
    Ch: CurrentChannelUpdater,
    C: UpdateChecker,
    A: UpdateApplier,
{
    fn clone(&self) -> Self {
        Self { update_manager: Arc::clone(&self.update_manager) }
    }
}

impl RealUpdateService {
    pub fn new(update_manager: Arc<RealUpdateManager>) -> Self {
        Self { update_manager }
    }
}

impl<T, Ch, C, A> UpdateService<T, Ch, C, A>
where
    T: TargetChannelUpdater,
    Ch: CurrentChannelUpdater,
    C: UpdateChecker,
    A: UpdateApplier,
{
    pub async fn handle_request_stream(
        &self,
        mut request_stream: ManagerRequestStream,
    ) -> Result<(), Error> {
        while let Some(event) =
            request_stream.try_next().await.context("error extracting request from stream")?
        {
            match event {
                ManagerRequest::CheckNow { options, monitor, responder } => {
                    self.handle_check_now(options, monitor, responder).await?;
                }
            }
        }
        Ok(())
    }

    async fn handle_check_now(
        &self,
        options: CheckOptions,
        monitor: Option<ClientEnd<MonitorMarker>>,
        responder: ManagerCheckNowResponder,
    ) -> Result<(), Error> {
        let initiator = match extract_initiator(&options) {
            Ok(initiator) => initiator,
            Err(e) => {
                responder
                    .send(&mut Err(CheckNotStartedReason::InvalidOptions))
                    .context("error sending CheckNow response")?;
                return Err(e);
            }
        };
        let update_state = self.update_manager.get_state().await;
        let notifier = match monitor {
            Some(monitor)
                if options.allow_attaching_to_existing_update_check == Some(true)
                    || update_state == None =>
            {
                Some(RealStateNotifier {
                    proxy: monitor.into_proxy().context("CheckNow monitor into_proxy")?,
                })
            }
            _ => None,
        };

        responder
            .send(
                &mut self
                    .update_manager
                    .try_start_update(
                        initiator,
                        notifier,
                        options.allow_attaching_to_existing_update_check,
                    )
                    .await,
            )
            .context("error sending CheckNow response")?;

        Ok(())
    }
}

fn extract_initiator(options: &CheckOptions) -> Result<Initiator, Error> {
    if let Some(initiator) = options.initiator {
        match initiator {
            fidl_fuchsia_update::Initiator::User => Ok(Initiator::Manual),
            fidl_fuchsia_update::Initiator::Service => Ok(Initiator::Automatic),
        }
    } else {
        return Err(format_err!("CheckNow options must specify initiator"));
    }
}

pub struct RealStateNotifier {
    proxy: fidl_fuchsia_update::MonitorProxy,
}

impl Notify<State> for RealStateNotifier {
    fn notify(&self, state: State) -> BoxFuture<'static, Result<(), ClosedClient>> {
        self.proxy
            .on_state(&mut state.into())
            .map(|result| result.map_err(|_| ClosedClient))
            .boxed()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::update_manager::tests::{
        BlockingUpdateChecker, FakeCurrentChannelUpdater, FakeLastUpdateStorage,
        FakeTargetChannelUpdater, FakeUpdateApplier, FakeUpdateChecker, LATEST_SYSTEM_IMAGE,
    };
    use fidl::endpoints::create_proxy_and_stream;
    use fidl_fuchsia_update::{
        Initiator, ManagerMarker, ManagerProxy, MonitorRequest, MonitorRequestStream,
    };
    use fidl_fuchsia_update_ext::{
        CheckOptionsBuilder, InstallationErrorData, InstallingData, UpdateInfo,
    };
    use fuchsia_async as fasync;
    use matches::assert_matches;

    async fn spawn_update_service<T, Ch, C, A>(
        channel_updater: T,
        current_channel_updater: Ch,
        update_checker: C,
        update_applier: A,
    ) -> (ManagerProxy, UpdateService<T, Ch, C, A>)
    where
        T: TargetChannelUpdater,
        Ch: CurrentChannelUpdater,
        C: UpdateChecker,
        A: UpdateApplier,
    {
        let update_service = UpdateService::<T, Ch, C, A> {
            update_manager: Arc::new(
                UpdateManager::<T, Ch, C, A, RealStateNotifier>::from_checker_and_applier(
                    Arc::new(channel_updater),
                    Arc::new(current_channel_updater),
                    update_checker,
                    update_applier,
                    FakeLastUpdateStorage::new(),
                )
                .await,
            ),
        };
        let update_service_clone = update_service.clone();
        let (proxy, stream) =
            create_proxy_and_stream::<ManagerMarker>().expect("create_proxy_and_stream");
        fasync::spawn(
            async move { update_service.handle_request_stream(stream).map(|_| ()).await },
        );
        (proxy, update_service_clone)
    }

    async fn collect_all_on_state_events(monitor: MonitorRequestStream) -> Vec<State> {
        monitor
            .map(|r| {
                let MonitorRequest::OnState { state, responder } = r.unwrap();
                responder.send().unwrap();
                state.into()
            })
            .collect()
            .await
    }

    async fn next_n_on_state_events(
        mut request_stream: MonitorRequestStream,
        n: usize,
    ) -> (MonitorRequestStream, Vec<State>) {
        let mut v = Vec::with_capacity(n);
        for _ in 0..n {
            let MonitorRequest::OnState { state, responder } =
                request_stream.next().await.unwrap().unwrap();
            responder.send().unwrap();
            v.push(state.into());
        }
        (request_stream, v)
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_check_now_monitor_sees_on_state_events() {
        let proxy = spawn_update_service(
            FakeTargetChannelUpdater::new(),
            FakeCurrentChannelUpdater::new(),
            FakeUpdateChecker::new_update_available(),
            FakeUpdateApplier::new_error(),
        )
        .await
        .0;
        let (client_end, request_stream) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        let expected_update_info = Some(UpdateInfo {
            version_available: Some(LATEST_SYSTEM_IMAGE.to_string()),
            download_size: None,
        });
        let options = CheckOptionsBuilder::new().initiator(Initiator::User).build();

        assert_matches!(proxy.check_now(options.into(), Some(client_end)).await.unwrap(), Ok(()));

        assert_eq!(
            collect_all_on_state_events(request_stream).await,
            vec![
                State::CheckingForUpdates,
                State::InstallingUpdate(InstallingData {
                    update: expected_update_info.clone(),
                    installation_progress: None,
                }),
                State::InstallationError(InstallationErrorData {
                    update: expected_update_info,
                    installation_progress: None,
                }),
            ]
        );
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_multiple_clients_see_on_state_events() {
        let (blocking_update_checker, unblocker) = BlockingUpdateChecker::new_checker_and_sender();
        let (proxy0, service) = spawn_update_service(
            FakeTargetChannelUpdater::new(),
            FakeCurrentChannelUpdater::new(),
            blocking_update_checker,
            FakeUpdateApplier::new_error(),
        )
        .await;
        let expected_update_info = Some(UpdateInfo {
            version_available: Some(LATEST_SYSTEM_IMAGE.to_string()),
            download_size: None,
        });

        let (proxy1, stream1) =
            create_proxy_and_stream::<ManagerMarker>().expect("create_proxy_and_stream");
        fasync::spawn(async move { service.handle_request_stream(stream1).map(|_| ()).await });

        let (client_end0, request_stream0) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        let (client_end1, request_stream1) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        let opt_builder = CheckOptionsBuilder::new()
            .initiator(Initiator::User)
            .allow_attaching_to_existing_update_check(true);

        // Add both monitor clients. We use a blocker to ensure we only start the update check when
        // both Monitor clients are enqueued. This prevents the second client from getting an
        // additional state event (since the event queue sends the last event when you add a client)
        assert_matches!(
            proxy0.check_now(opt_builder.clone().build(), Some(client_end0)).await.unwrap(),
            Ok(())
        );
        assert_matches!(
            proxy1.check_now(opt_builder.build(), Some(client_end1)).await.unwrap(),
            Ok(())
        );
        assert_matches!(unblocker.send(()), Ok(()));

        let events = next_n_on_state_events(request_stream0, 3).await.1;
        assert_eq!(
            events,
            vec![
                State::CheckingForUpdates,
                State::InstallingUpdate(InstallingData {
                    update: expected_update_info.clone(),
                    installation_progress: None,
                }),
                State::InstallationError(InstallationErrorData {
                    update: expected_update_info.clone(),
                    installation_progress: None,
                }),
            ]
        );

        assert_eq!(
            collect_all_on_state_events(request_stream1).await,
            vec![
                State::CheckingForUpdates,
                State::InstallingUpdate(InstallingData {
                    update: expected_update_info.clone(),
                    installation_progress: None,
                }),
                State::InstallationError(InstallationErrorData {
                    update: expected_update_info.clone(),
                    installation_progress: None,
                }),
            ]
        );
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_check_now_monitor_already_in_progress() {
        let (blocking_update_checker, unblocker) = BlockingUpdateChecker::new_checker_and_sender();
        let proxy = spawn_update_service(
            FakeTargetChannelUpdater::new(),
            FakeCurrentChannelUpdater::new(),
            blocking_update_checker,
            FakeUpdateApplier::new_error(),
        )
        .await
        .0;
        let expected_update_info = Some(UpdateInfo {
            version_available: Some(LATEST_SYSTEM_IMAGE.to_string()),
            download_size: None,
        });
        let (client_end0, request_stream0) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        let (client_end1, request_stream1) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        let opt_builder = CheckOptionsBuilder::new()
            .initiator(Initiator::User)
            .allow_attaching_to_existing_update_check(false);

        //Start a hang on InstallingUpdate
        assert_matches!(
            proxy.check_now(opt_builder.clone().build(), Some(client_end0)).await.unwrap(),
            Ok(())
        );

        // When we do the next check, we should get an already in progress error since we're not
        // allowed to attach another client
        assert_eq!(
            proxy.check_now(opt_builder.build(), Some(client_end1)).await.unwrap(),
            Err(CheckNotStartedReason::AlreadyInProgress)
        );

        // When we resume, only the first client should see the on state events
        assert_matches!(unblocker.send(()), Ok(()));
        assert_eq!(
            collect_all_on_state_events(request_stream0).await,
            vec![
                State::CheckingForUpdates,
                State::InstallingUpdate(InstallingData {
                    update: expected_update_info.clone(),
                    installation_progress: None,
                }),
                State::InstallationError(InstallationErrorData {
                    update: expected_update_info,
                    installation_progress: None,
                }),
            ]
        );
        assert_eq!(collect_all_on_state_events(request_stream1).await, vec![]);
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_check_now_invalid_options() {
        let proxy = spawn_update_service(
            FakeTargetChannelUpdater::new(),
            FakeCurrentChannelUpdater::new(),
            FakeUpdateChecker::new_update_available(),
            FakeUpdateApplier::new_error(),
        )
        .await
        .0;
        let (client_end, request_stream) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        // Default has no initiator
        let options = CheckOptionsBuilder::new().build();

        let res = proxy.check_now(options, Some(client_end)).await.unwrap();

        assert_eq!(res, Err(CheckNotStartedReason::InvalidOptions));
        assert_eq!(collect_all_on_state_events(request_stream).await, vec![]);
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_check_now_monitor_already_in_progress_but_allow_attaching_to_existing_update_check(
    ) {
        let (blocking_update_checker, unblocker) = BlockingUpdateChecker::new_checker_and_sender();
        let proxy = spawn_update_service(
            FakeTargetChannelUpdater::new(),
            FakeCurrentChannelUpdater::new(),
            blocking_update_checker,
            FakeUpdateApplier::new_error(),
        )
        .await
        .0;
        let (client_end0, request_stream0) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        let (client_end1, request_stream1) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        let opt_builder = CheckOptionsBuilder::new()
            .initiator(Initiator::User)
            .allow_attaching_to_existing_update_check(true);
        let expected_update_info = Some(UpdateInfo {
            version_available: Some(LATEST_SYSTEM_IMAGE.to_string()),
            download_size: None,
        });
        let expected_states = vec![
            State::CheckingForUpdates,
            State::InstallingUpdate(InstallingData {
                update: expected_update_info.clone(),
                installation_progress: None,
            }),
            State::InstallationError(InstallationErrorData {
                update: expected_update_info,
                installation_progress: None,
            }),
        ];

        // Start a hang on InstallingUpdate
        assert_matches!(
            proxy.check_now(opt_builder.clone().build(), Some(client_end0)).await.unwrap(),
            Ok(())
        );

        // When we do the next check, we should get an OK since we're allowed to attach to
        // an existing check
        assert_matches!(
            proxy.check_now(opt_builder.build(), Some(client_end1)).await.unwrap(),
            Ok(())
        );

        // When we resume, both clients should see the on state events
        assert_matches!(unblocker.send(()), Ok(()));
        assert_eq!(collect_all_on_state_events(request_stream0).await, expected_states.clone());
        assert_eq!(collect_all_on_state_events(request_stream1).await, expected_states);
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_update_attempt_persists_across_client_disconnect_reconnect() {
        let (blocking_update_checker, unblocker) = BlockingUpdateChecker::new_checker_and_sender();
        let fake_update_applier = FakeUpdateApplier::new_error();
        let (proxy0, service) = spawn_update_service(
            FakeTargetChannelUpdater::new(),
            FakeCurrentChannelUpdater::new(),
            blocking_update_checker,
            fake_update_applier.clone(),
        )
        .await;
        let expected_update_info = Some(UpdateInfo {
            version_available: Some(LATEST_SYSTEM_IMAGE.to_string()),
            download_size: None,
        });

        let (client_end0, request_stream0) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        let (client_end1, request_stream1) =
            fidl::endpoints::create_request_stream().expect("create_request_stream");
        let opt_builder = CheckOptionsBuilder::new()
            .initiator(Initiator::User)
            .allow_attaching_to_existing_update_check(true);

        assert_matches!(
            proxy0.check_now(opt_builder.clone().build(), Some(client_end0)).await.unwrap(),
            Ok(())
        );

        let (_, events) = next_n_on_state_events(request_stream0, 1).await;
        assert_eq!(events, vec![State::CheckingForUpdates]);

        drop(proxy0);

        let (proxy1, stream1) =
            create_proxy_and_stream::<ManagerMarker>().expect("create_proxy_and_stream");
        fasync::spawn(async move { service.handle_request_stream(stream1).map(|_| ()).await });

        // The first update check is still in progress and blocked, but we'll get an OK
        // since we allow_attaching_to_existing_update_check=true
        assert_matches!(
            proxy1.check_now(opt_builder.build(), Some(client_end1)).await.unwrap(),
            Ok(())
        );

        // Once we unblock, the update should resume
        assert_matches!(unblocker.send(()), Ok(()));
        assert_eq!(
            collect_all_on_state_events(request_stream1).await,
            vec![
                // the second request stream gets this since the event queue sent the last event :)
                State::CheckingForUpdates,
                State::InstallingUpdate(InstallingData {
                    update: expected_update_info.clone(),
                    installation_progress: None,
                }),
                State::InstallationError(InstallationErrorData {
                    update: expected_update_info,
                    installation_progress: None,
                }),
            ]
        );
        assert_eq!(fake_update_applier.call_count(), 1);
    }
}
