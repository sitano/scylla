/*
 * Copyright (C) 2020 ScyllaDB
 */

/*
 * This file is part of Scylla.
 *
 * Scylla is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Scylla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Scylla.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <seastar/testing/test_case.hh>

#include "tracing/tracing.hh"
#include "tracing/trace_state.hh"
#include "tracing/tracing_backend_registry.hh"

#include "test/lib/cql_test_env.hh"

SEASTAR_TEST_CASE(tracing_fast_mode) {
    return do_with_cql_env([](auto &e) {
        // supervisor::notify("creating tracing");
        tracing::backend_registry tracing_backend_registry;
        tracing::register_tracing_keyspace_backend(tracing_backend_registry);
        tracing::tracing::create_tracing(tracing_backend_registry, "trace_keyspace_helper").get();

        // supervisor::notify("starting tracing");
        tracing::tracing::start_tracing(e.qp()).get();

        // create session
        tracing::trace_state_props_set trace_props;
        trace_props.set(tracing::trace_state_props::log_slow_query);
        trace_props.set(tracing::trace_state_props::full_tracing);

        tracing::tracing &t = tracing::tracing::get_local_tracing_instance();
        tracing::trace_state_ptr trace_state = t.create_session(tracing::trace_type::QUERY, trace_props);

        // begin
        tracing::begin(trace_state, "begin", gms::inet_address());

        // disable tracing events
        BOOST_CHECK(!t.ignore_trace_events_enabled());
        t.set_ignore_trace_events(true);
        BOOST_CHECK(t.ignore_trace_events_enabled());

        // check no event created
        tracing::trace(trace_state, "trace 1");
        BOOST_CHECK_EQUAL(trace_state->records_size(), 0);
        BOOST_CHECK(tracing::make_trace_info(trace_state) == std::nullopt);

        // enable back tracing events
        t.set_ignore_trace_events(false);
        BOOST_CHECK(!t.ignore_trace_events_enabled());

        // create an event
        tracing::trace(trace_state, "trace 1");
        BOOST_CHECK_EQUAL(trace_state->records_size(), 1);
        BOOST_CHECK(tracing::make_trace_info(trace_state) != std::nullopt);

        return make_ready_future<>();
    });
}
