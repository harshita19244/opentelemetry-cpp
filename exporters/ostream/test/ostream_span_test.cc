#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/span_data.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/provider.h"

#include "opentelemetry/sdk/trace/exporter.h"

#include "opentelemetry/exporters/ostream/span_exporter.h"

#include "ostream_capture.h"

#include <gtest/gtest.h>
#include <iostream>

using namespace opentelemetry::exporter::ostream::test;

namespace trace    = opentelemetry::trace;
namespace common   = opentelemetry::common;
namespace nostd    = opentelemetry::nostd;
namespace sdktrace = opentelemetry::sdk::trace;

using Attributes = std::initializer_list<std::pair<nostd::string_view, common::AttributeValue>>;

// Testing Shutdown functionality of OStreamSpanExporter, should expect no data to be sent to Stream
TEST(OStreamSpanExporter, Shutdown)
{
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter);
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();
  recordable->SetName("Test Span");

  // Capture the output of cout
  const auto captured = WithOStreamCapture(std::cout, [&]() {
    EXPECT_TRUE(processor->Shutdown());
    processor->OnEnd(std::move(recordable));
  });

  EXPECT_EQ(captured, "");
}

constexpr const char *kDefaultSpanPrinted =
    "{\n"
    "  name          : \n"
    "  trace_id      : 00000000000000000000000000000000\n"
    "  span_id       : 0000000000000000\n"
    "  parent_span_id: 0000000000000000\n"
    "  start         : 0\n"
    "  duration      : 0\n"
    "  description   : \n"
    "  span kind     : Internal\n"
    "  status        : Unset\n"
    "  attributes    : \n"
    "  events        : \n"
    "  links         : \n"
    "}\n";

// Testing what a default span that is not changed will print out, either all 0's or empty values
TEST(OStreamSpanExporter, PrintDefaultSpan)
{
  std::stringstream output;
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter(output));
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();

  processor->OnEnd(std::move(recordable));

  EXPECT_EQ(output.str(), kDefaultSpanPrinted);
}

TEST(OStreamSpanExporter, PrintSpanWithBasicFields)
{
  std::stringstream output;
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter(output));
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();

  recordable->SetName("Test Span");
  opentelemetry::core::SystemTimestamp now(std::chrono::system_clock::now());

  constexpr uint8_t trace_id_buf[] = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8};
  opentelemetry::trace::TraceId t_id(trace_id_buf);
  constexpr uint8_t span_id_buf[] = {1, 2, 3, 4, 5, 6, 7, 8};
  opentelemetry::trace::SpanId s_id(span_id_buf);

  recordable->SetIds(t_id, s_id, s_id);

  recordable->SetStartTime(now);
  recordable->SetDuration(std::chrono::nanoseconds(100));
  recordable->SetStatus(opentelemetry::trace::StatusCode::kOk, "Test Description");
  recordable->SetSpanKind(opentelemetry::trace::SpanKind::kClient);

  processor->OnEnd(std::move(recordable));

  std::string start = std::to_string(now.time_since_epoch().count());

  std::string expectedOutput =
      "{\n"
      "  name          : Test Span\n"
      "  trace_id      : 01020304050607080102030405060708\n"
      "  span_id       : 0102030405060708\n"
      "  parent_span_id: 0102030405060708\n"
      "  start         : " +
      start +
      "\n"
      "  duration      : 100\n"
      "  description   : Test Description\n"
      "  span kind     : Client\n"
      "  status        : Ok\n"
      "  attributes    : \n"
      "  events        : \n"
      "  links         : \n"
      "}\n";
  EXPECT_EQ(output.str(), expectedOutput);
}

TEST(OStreamSpanExporter, PrintSpanWithAttribute)
{
  std::stringstream output;
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter(output));
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();

  recordable->SetAttribute("attr1", "string");

  processor->OnEnd(std::move(recordable));

  std::string expectedOutput =
      "{\n"
      "  name          : \n"
      "  trace_id      : 00000000000000000000000000000000\n"
      "  span_id       : 0000000000000000\n"
      "  parent_span_id: 0000000000000000\n"
      "  start         : 0\n"
      "  duration      : 0\n"
      "  description   : \n"
      "  span kind     : Internal\n"
      "  status        : Unset\n"
      "  attributes    : \n"
      "\tattr1: string\n"
      "  events        : \n"
      "  links         : \n"
      "}\n";
  EXPECT_EQ(output.str(), expectedOutput);
}

TEST(OStreamSpanExporter, PrintSpanWithArrayAttribute)
{
  std::stringstream output;
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter(output));
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();

  std::array<int, 3> array1 = {1, 2, 3};
  opentelemetry::nostd::span<int> span1{array1.data(), array1.size()};
  recordable->SetAttribute("array1", span1);

  processor->OnEnd(std::move(recordable));

  std::string expectedOutput =
      "{\n"
      "  name          : \n"
      "  trace_id      : 00000000000000000000000000000000\n"
      "  span_id       : 0000000000000000\n"
      "  parent_span_id: 0000000000000000\n"
      "  start         : 0\n"
      "  duration      : 0\n"
      "  description   : \n"
      "  span kind     : Internal\n"
      "  status        : Unset\n"
      "  attributes    : \n"
      "\tarray1: [1,2,3]\n"
      "  events        : \n"
      "  links         : \n"
      "}\n";
  EXPECT_EQ(output.str(), expectedOutput);
}

TEST(OStreamSpanExporter, PrintSpanWithEvents)
{
  std::stringstream output;
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter(output));
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();
  opentelemetry::core::SystemTimestamp now(std::chrono::system_clock::now());
  opentelemetry::core::SystemTimestamp next(std::chrono::system_clock::now() +
                                            std::chrono::seconds(1));

  std::string now_str  = std::to_string(now.time_since_epoch().count());
  std::string next_str = std::to_string(next.time_since_epoch().count());

  recordable->AddEvent("hello", now);
  recordable->AddEvent("world", next,
                       common::KeyValueIterableView<Attributes>({{"attr1", "string"}}));

  processor->OnEnd(std::move(recordable));

  std::string expectedOutput =
      "{\n"
      "  name          : \n"
      "  trace_id      : 00000000000000000000000000000000\n"
      "  span_id       : 0000000000000000\n"
      "  parent_span_id: 0000000000000000\n"
      "  start         : 0\n"
      "  duration      : 0\n"
      "  description   : \n"
      "  span kind     : Internal\n"
      "  status        : Unset\n"
      "  attributes    : \n"
      "  events        : \n"
      "\t{\n"
      "\t  name          : hello\n"
      "\t  timestamp     : " +
      now_str +
      "\n"
      "\t  attributes    : \n"
      "\t}\n"
      "\t{\n"
      "\t  name          : world\n"
      "\t  timestamp     : " +
      next_str +
      "\n"
      "\t  attributes    : \n"
      "\t\tattr1: string\n"
      "\t}\n"
      "  links         : \n"
      "}\n";
  EXPECT_EQ(output.str(), expectedOutput);
}

TEST(OStreamSpanExporter, PrintSpanWithLinks)
{
  std::stringstream output;
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter(output));
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();

  // produce valid SpanContext with pseudo span and trace Id.
  uint8_t span_id_buf[opentelemetry::trace::SpanId::kSize] = {
      1,
  };
  opentelemetry::trace::SpanId span_id{span_id_buf};
  uint8_t trace_id_buf[opentelemetry::trace::TraceId::kSize] = {
      2,
  };
  opentelemetry::trace::TraceId trace_id{trace_id_buf};
  const auto span_context = opentelemetry::trace::SpanContext(
      trace_id, span_id,
      opentelemetry::trace::TraceFlags{opentelemetry::trace::TraceFlags::kIsSampled}, true);

  // and another to check preserving order.
  uint8_t span_id_buf2[opentelemetry::trace::SpanId::kSize] = {
      3,
  };
  opentelemetry::trace::SpanId span_id2{span_id_buf2};
  const auto span_context2 = opentelemetry::trace::SpanContext(
      trace_id, span_id2,
      opentelemetry::trace::TraceFlags{opentelemetry::trace::TraceFlags::kIsSampled}, true,
      opentelemetry::trace::TraceState::FromHeader("state1=value"));

  recordable->AddLink(span_context);
  recordable->AddLink(span_context2,
                      common::KeyValueIterableView<Attributes>({{"attr1", "string"}}));

  processor->OnEnd(std::move(recordable));

  std::string expectedOutput =
      "{\n"
      "  name          : \n"
      "  trace_id      : 00000000000000000000000000000000\n"
      "  span_id       : 0000000000000000\n"
      "  parent_span_id: 0000000000000000\n"
      "  start         : 0\n"
      "  duration      : 0\n"
      "  description   : \n"
      "  span kind     : Internal\n"
      "  status        : Unset\n"
      "  attributes    : \n"
      "  events        : \n"
      "  links         : \n"
      "\t{\n"
      "\t  trace_id      : 02000000000000000000000000000000\n"
      "\t  span_id       : 0100000000000000\n"
      "\t  tracestate    : \n"
      "\t  attributes    : \n"
      "\t}\n"
      "\t{\n"
      "\t  trace_id      : 02000000000000000000000000000000\n"
      "\t  span_id       : 0300000000000000\n"
      "\t  tracestate    : state1=value\n"
      "\t  attributes    : \n"
      "\t\tattr1: string\n"
      "\t}\n"
      "}\n";
  EXPECT_EQ(output.str(), expectedOutput);
}

// Test with the three common ostreams, tests are more of a sanity check and usage examples.
TEST(OStreamSpanExporter, PrintSpanToCout)
{
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter);
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();

  const auto captured =
      WithOStreamCapture(std::cout, [&]() { processor->OnEnd(std::move(recordable)); });

  EXPECT_EQ(captured, kDefaultSpanPrinted);
}

TEST(OStreamSpanExporter, PrintSpanToCerr)
{
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter(std::cerr));
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();

  const auto captured =
      WithOStreamCapture(std::cerr, [&]() { processor->OnEnd(std::move(recordable)); });

  EXPECT_EQ(captured, kDefaultSpanPrinted);
}

TEST(OStreamSpanExporter, PrintSpanToClog)
{
  auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
      new opentelemetry::exporter::trace::OStreamSpanExporter(std::clog));
  auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));

  auto recordable = processor->MakeRecordable();

  const auto captured =
      WithOStreamCapture(std::clog, [&]() { processor->OnEnd(std::move(recordable)); });

  EXPECT_EQ(captured, kDefaultSpanPrinted);
}
