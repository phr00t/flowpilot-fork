#include <assert.h>
#include <sched.h>

#include "cereal/messaging/messaging.h"
#include "selfdrive/modeld/models/driving.h"
#include "common/util.h"


ExitHandler do_exit;
float model_raw_preds[NET_OUTPUT_SIZE];

int main(int argc, char **argv) {
  AlignedBuffer aligned_buf;

  PubMaster pm({"modelV2", "cameraOdometry"});

  std::unique_ptr<Context> context(Context::create());
  std::unique_ptr<SubSocket> subscriber(SubSocket::create(context.get(), "modelRaw"));
  assert(subscriber != NULL);

  uint32_t last_frame_id = 0;

  while (!do_exit) {
    // this receive should block
    std::unique_ptr<Message> msg(subscriber->receive());
    if (!msg) {
      if (errno == EINTR) {
        do_exit = true;
      }
      continue;
    }

    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(msg.get()));
    cereal::Event::Reader event = cmsg.getRoot<cereal::Event>();
    auto modelRaw = event.getModelRaw();
    auto model_raw = modelRaw.getRawPredictions();

    assert(model_raw.size() == NET_OUTPUT_SIZE);
    for (int i=0; i<NET_OUTPUT_SIZE; i++)
      model_raw_preds[i] = model_raw[i];

    uint32_t vipc_dropped_frames = modelRaw.getFrameId() - last_frame_id - 1;
    
    model_publish(pm, modelRaw.getFrameId(), modelRaw.getFrameIdExtra(), modelRaw.getFrameId(), modelRaw.getFrameDropPerc()/100, 
                  model_raw_preds, modelRaw.getTimestampEof(), modelRaw.getModelExecutionTime(), modelRaw.getValid());
    posenet_publish(pm, modelRaw.getFrameId(), vipc_dropped_frames, model_raw_preds, modelRaw.getTimestampEof(), modelRaw.getValid());

    last_frame_id = modelRaw.getFrameId();
  }
  return 0;
}
