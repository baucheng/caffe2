#include "caffe2/core/db.h"
#include "caffe2/core/init.h"
#include "caffe2/proto/caffe2.pb.h"
#include "caffe/proto/caffe.pb.h"
#include "caffe2/core/logging.h"

CAFFE2_DEFINE_string(input_db, "", "The input db.");
CAFFE2_DEFINE_string(input_db_type, "", "The input db type.");
CAFFE2_DEFINE_string(output_db, "", "The output db.");
CAFFE2_DEFINE_string(output_db_type, "", "The output db type.");
CAFFE2_DEFINE_int(batch_size, 1000, "The write batch size.");

using caffe2::db::Cursor;
using caffe2::db::DB;
using caffe2::db::Transaction;
using caffe2::TensorProto;
using caffe2::TensorProtos;

int main(int argc, char** argv) {
  caffe2::GlobalInit(&argc, argv);

  std::unique_ptr<DB> in_db(caffe2::db::CreateDB(
      caffe2::FLAGS_input_db_type, caffe2::FLAGS_input_db, caffe2::db::READ));
  std::unique_ptr<DB> out_db(caffe2::db::CreateDB(
      caffe2::FLAGS_output_db_type, caffe2::FLAGS_output_db, caffe2::db::NEW));
  std::unique_ptr<Cursor> cursor(in_db->NewCursor());
  std::unique_ptr<Transaction> transaction(out_db->NewTransaction());
  int count = 0;
  for (; cursor->Valid(); cursor->Next()) {
    caffe::Datum datum;
    CAFFE_CHECK(datum.ParseFromString(cursor->value()));
    TensorProtos protos;
    TensorProto* data = protos.add_protos();
    TensorProto* label = protos.add_protos();
    label->set_data_type(TensorProto::INT32);
    label->add_dims(1);
    label->add_int32_data(datum.label());
    if (datum.encoded()) {
      // This is an encoded image. we will copy over the data directly.
      data->set_data_type(TensorProto::STRING);
      data->add_dims(1);
      data->add_string_data(datum.data());
    } else {
      // float data not supported right now.
      CAFFE_CHECK_EQ(datum.float_data_size(), 0);
      char buffer[datum.data().size()];
      // swap order from CHW to HWC
      int channels = datum.channels();
      int size = datum.height() * datum.width();
      CAFFE_CHECK_EQ(datum.data().size(), channels * size);
      for (int c = 0; c < channels; ++c) {
        char* dst = buffer + c;
        const char* src = datum.data().c_str() + c * size;
        for (int n = 0; n < size; ++n) {
          dst[n*channels] = src[n];
        }
      }
      data->set_data_type(TensorProto::BYTE);
      data->add_dims(datum.height());
      data->add_dims(datum.width());
      data->add_dims(datum.channels());
      data->set_byte_data(buffer, datum.data().size());
    }
    transaction->Put(cursor->key(), protos.SerializeAsString());
    if (++count % caffe2::FLAGS_batch_size == 0) {
      transaction->Commit();
      CAFFE_LOG_INFO << "Converted " << count << " items so far.";
    }
  }
  CAFFE_LOG_INFO << "A total of " << count << " items processed.";
  return 0;
}

