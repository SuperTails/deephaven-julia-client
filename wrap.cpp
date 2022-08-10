#include <arrow/buffer.h>
#include <arrow/io/type_fwd.h>
#include <arrow/io/memory.h>
#include <arrow/ipc/reader.h>
#include <arrow/ipc/writer.h>
#include <arrow/record_batch.h>
#include <arrow/table.h>

#include <arrow/status.h>
#include <arrow/type_fwd.h>
#include <arrow/util/string_view.h>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <jlcxx/array.hpp>
#include <julia.h>
#include <memory>
#include <string>
#include <vector>

#include "jlcxx/jlcxx.hpp"
#include "jlcxx/const_array.hpp"
#include "jlcxx/stl.hpp"

#include "deephaven/client/client.h"
#include "deephaven/client/utility/utility.h"

using deephaven::client::Client;
using deephaven::client::TableHandleManager;
using deephaven::client::TableHandle;
using deephaven::client::internal::TableHandleStreamAdaptor;
using deephaven::client::utility::okOrThrow;

std::string greet() {
	return "hello, world!";
}

int connect_to_server() {
  const char *server = "localhost:10000";
  try {
    auto client = Client::connect(server);
    auto manager = client.getManager();
    auto table = manager.emptyTable(10);
    auto t2 = table.update("ABC = ii + 100");
    std::cout << t2.stream(true) << '\n';
  } catch (const std::exception &e) {
    std::cerr << "Caught exception: " << e.what() << '\n';
  }
  return 0;
}

std::string showTable(const TableHandle &table, bool wantHeaders) {
	std::ostringstream s;
	s << table.stream(wantHeaders);
	return s.str();
}

class ChunkInterface {
public:
  ChunkInterface(const TableHandle &tableHandle) :
    reader(tableHandle.getFlightStreamReader())
  {
    Next();
  }

  void Next() {
    okOrThrow(DEEPHAVEN_EXPR_MSG(reader->Next(&chunk)));
  }

  jlcxx::ConstArray< int64_t, 1 > columnAsInt64(int col) {
    auto column = chunk.data->column(col);
    auto arr_int64 = std::dynamic_pointer_cast<arrow::NumericArray<arrow::Int64Type>>(column);
    if (arr_int64 != nullptr) {
      if (arr_int64->null_count() != 0) {
        std::cout << "TODO: NULLS";
      }

      return jlcxx::make_const_array(arr_int64->raw_values(), arr_int64->length());
    } else {
      return nullptr;
    }
  }

  jlcxx::ConstArray< int32_t, 1 > columnAsInt32(int col) {
    auto column = chunk.data->column(col);
    auto arr_int32 = std::dynamic_pointer_cast<arrow::NumericArray<arrow::Int32Type>>(column);
    if (arr_int32 != nullptr) {
      if (arr_int32->null_count() != 0) {
        std::cout << "TODO: NULLS";
      }

      return jlcxx::make_const_array(arr_int32->raw_values(), arr_int32->length());
    } else {
      return nullptr;
    }
  }

private:
  std::shared_ptr<arrow::flight::FlightStreamReader> reader;

  arrow::flight::FlightStreamChunk chunk;
};

std::shared_ptr<arrow::Table> deserializeTable(jlcxx::ArrayRef<uint8_t, 1> data) {
  std::vector<unsigned char> d2;
  for (int8_t elem : data) {
    d2.push_back(elem);
  }

  auto buf_reader = arrow::io::BufferReader(d2.data(), d2.size());

  auto str_reader = arrow::ipc::RecordBatchStreamReader::Open(&buf_reader).ValueOrDie();

  std::shared_ptr<arrow::Table> tbl = nullptr;

  okOrThrow(DEEPHAVEN_EXPR_MSG(str_reader->ReadAll(&tbl)));

  return tbl;
}

TableHandle uploadTable(const TableHandleManager &manager, arrow::Table &table) {
  auto wrapper = manager.createFlightWrapper();
  auto [result, fd] = manager.newTableHandleAndFlightDescriptor();

  arrow::flight::FlightCallOptions options;
  wrapper.addAuthHeaders(&options);

  std::unique_ptr<arrow::flight::FlightStreamWriter> fsw;
  std::unique_ptr<arrow::flight::FlightMetadataReader> fmr;
  okOrThrow(DEEPHAVEN_EXPR_MSG(wrapper.flightClient()->DoPut(options, fd, table.schema(), &fsw, &fmr)));

  okOrThrow(DEEPHAVEN_EXPR_MSG(fsw->WriteTable(table)));
  okOrThrow(DEEPHAVEN_EXPR_MSG(fsw->DoneWriting()));

  std::shared_ptr<arrow::Buffer> buf;
  okOrThrow(DEEPHAVEN_EXPR_MSG(fmr->ReadMetadata(&buf)));
  okOrThrow(DEEPHAVEN_EXPR_MSG(fsw->Close()));

  return result;
}

TableHandle deserializeAndUpload(const TableHandleManager &manager, jlcxx::ArrayRef<uint8_t, 1> data) {
  auto tbl = deserializeTable(data);
  return uploadTable(manager, *tbl);
}

class TableSerializer {
public:
  TableSerializer(const TableHandle &tableHandle) {
    auto s = arrow::io::BufferOutputStream::Create();
    auto stream = s.ValueOrDie();

    auto fsr = tableHandle.getFlightStreamReader();

    auto schema = fsr->GetSchema().ValueOrDie();

    auto writer = arrow::ipc::MakeStreamWriter(stream, schema).ValueOrDie();

    while (true) {
      arrow::flight::FlightStreamChunk chunk;
      okOrThrow(DEEPHAVEN_EXPR_MSG(fsr->Next(&chunk)));
      if (chunk.data == nullptr) {
        break;
      }

      okOrThrow(DEEPHAVEN_EXPR_MSG(writer->WriteRecordBatch(*chunk.data)));
    }

    okOrThrow(DEEPHAVEN_EXPR_MSG(writer->Close()));

    result = stream->Finish().ValueOrDie();
  }

  jlcxx::ConstArray<uint8_t, 1> DataArr() {
    return jlcxx::make_const_array(this->Data(), this->DataLen());
  }

  const uint8_t *Data() {
    return result->data();
  }

  size_t DataLen() {
    return result->size();
  }
private:
  std::shared_ptr<arrow::Buffer> result;
};

JLCXX_MODULE define_julia_module(jlcxx::Module &mod) {
	mod.add_type<TableHandle>("TableHandle")
		.method("update", static_cast<TableHandle (TableHandle::*)(std::vector<std::string>) const>(&TableHandle::update))
		.method("show", &showTable);

	mod.add_type<TableHandleManager>("TableHandleManager")
		.method("emptyTable", &TableHandleManager::emptyTable);

	mod.add_type<Client>("Client")
		.method("getManager", &Client::getManager);

  mod.add_type<ChunkInterface>("ChunkInterface")
    .constructor<const TableHandle&>(true)
    .method("Next", &ChunkInterface::Next)
    .method("columnasint64", &ChunkInterface::columnAsInt64)
    .method("columnasint32", &ChunkInterface::columnAsInt32);

  mod.add_type<TableSerializer>("TableSerializer")
    .constructor<const TableHandle&>(true)
    .method("dataarr", &TableSerializer::DataArr);
  
	/*	.method("connect", &Client::connect);*/
		
	mod.method("clientconnect", &Client::connect);

  mod.method("deserializeandupload", &deserializeAndUpload);
		
	mod.method("greet", &greet);
	mod.method("connect_to_server", &connect_to_server);

  //mod.method("dumpsymbolcolumn", &dumpSymbolColumn);
}
