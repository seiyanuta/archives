#include "pci.h"
#include <resea.h>
#include <resea/cpp/memory.h>
#include <resea/pci.h>
#include "pci.h"


namespace pci {
namespace pci_server {

/** handles pci.listen */
void handle_listen(channel_t __ch, channel_t ch, uint32_t vendor, uint32_t device,
                    uint32_t subvendor, uint32_t subdevice) {

    // TODO
    resea::interfaces::pci::send_listen_reply(__ch, OK);

    void *config = allocate_memory(PCI_CONFIG_HEADER_SIZE, resea::interfaces::memory::ALLOC_NORMAL);
    if (pci_lookup(config, vendor, device, subvendor, subdevice) == OK) {
        INFO("found a device");
        resea::interfaces::pci::sendas_new_device(ch, config, PAYLOAD_MOVE_OOL, PCI_CONFIG_HEADER_SIZE, PAYLOAD_INLINE);
    } else {
        INFO("device not found");
    }
}

} // namespace pci_server
} // namespace pci
