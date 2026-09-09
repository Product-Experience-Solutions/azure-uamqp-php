/* Exercise the real dependency implementation; replace only session transport. */
#include <assert.h>
#include <stdio.h>

#define session_send_flow credit_test_send_flow
#define session_send_disposition credit_test_send_disposition
#include "src/link.c"
#include "azure_uamqp_c/messaging.h"

static unsigned int flow_count;
static unsigned int callback_count;
static unsigned int disposition_count;
static uint32_t advertised_credit;
static bool advertised_drain;
static uint32_t last_payload_size;

int credit_test_send_flow(LINK_ENDPOINT_HANDLE endpoint, FLOW_HANDLE flow)
{
    (void)endpoint;
    assert(flow_get_link_credit(flow, &advertised_credit) == 0);
    advertised_drain = false;
    (void)flow_get_drain(flow, &advertised_drain);
    flow_count++;
    return 0;
}

int credit_test_send_disposition(LINK_ENDPOINT_HANDLE endpoint, DISPOSITION_HANDLE disposition)
{
    bool settled = false;
    (void)endpoint;
    assert(disposition_get_settled(disposition, &settled) == 0 && settled);
    disposition_count++;
    return 0;
}

static AMQP_VALUE on_transfer(void* context, TRANSFER_HANDLE transfer, uint32_t size, const unsigned char* bytes)
{
    (void)context;
    (void)transfer;
    assert(bytes != NULL);
    last_payload_size = size;
    callback_count++;
    return messaging_delivery_accepted();
}

static void initialize_link(LINK_INSTANCE* link)
{
    memset(link, 0, sizeof(*link));
    link->role = role_receiver;
    link->link_state = LINK_STATE_HALF_ATTACHED_ATTACH_SENT;
    link->max_link_credit = DEFAULT_LINK_CREDIT;
    link->on_transfer_received = on_transfer;
    flow_count = callback_count = disposition_count = 0;
    advertised_credit = last_payload_size = 0;
    advertised_drain = false;
}

static void receive_attach(LINK_INSTANCE* link, uint32_t initial_count)
{
    ATTACH_HANDLE attach = attach_create("credit-test", 1, role_sender);
    AMQP_VALUE performative;
    assert(attach != NULL);
    assert(attach_set_initial_delivery_count(attach, initial_count) == 0);
    assert(attach_set_max_message_size(attach, 1024) == 0);
    performative = amqpvalue_create_attach(attach);
    assert(performative != NULL);
    link_frame_received(link, performative, 0, NULL);
    amqpvalue_destroy(performative);
    attach_destroy(attach);
    assert(link->link_state == LINK_STATE_ATTACHED);
}

static void receive_transfer(LINK_INSTANCE* link, uint32_t id, bool more, bool continuation)
{
    static const unsigned char payload[] = {'a', 'b'};
    TRANSFER_HANDLE transfer = transfer_create(1);
    AMQP_VALUE performative;
    assert(transfer != NULL);
    if (!continuation)
    {
        assert(transfer_set_delivery_id(transfer, id) == 0);
    }
    assert(transfer_set_more(transfer, more) == 0);
    performative = amqpvalue_create_transfer(transfer);
    assert(performative != NULL);
    link_frame_received(link, performative, sizeof(payload), payload);
    amqpvalue_destroy(performative);
    transfer_destroy(transfer);
}

static void receive_flow(LINK_INSTANCE* link, uint32_t count, uint32_t credit, bool drain)
{
    FLOW_HANDLE flow = flow_create(0, 0, 0);
    AMQP_VALUE performative;
    assert(flow != NULL);
    assert(flow_set_handle(flow, 1) == 0);
    assert(flow_set_delivery_count(flow, count) == 0);
    assert(flow_set_link_credit(flow, credit) == 0);
    assert(flow_set_drain(flow, drain) == 0);
    performative = amqpvalue_create_flow(flow);
    assert(performative != NULL);
    link_frame_received(link, performative, 0, NULL);
    amqpvalue_destroy(performative);
    flow_destroy(flow);
}

static bool drain_complete(LINK_INSTANCE* link)
{
    bool complete = true;
    assert(link_get_drain_complete(link, &complete) == 0);
    return complete;
}

static void finite_allowance(uint32_t count)
{
    LINK_INSTANCE link;
    uint32_t index;
    initialize_link(&link);
    assert(link_set_receiver_credit(&link, count) == 0);
    assert(flow_count == 0);
    receive_attach(&link, 0);
    assert(advertised_credit == count && flow_count == 1);
    for (index = 0; index < count; index++)
    {
        receive_transfer(&link, index, false, false);
        assert(link.current_link_credit == count - index - 1);
        assert(flow_count == 1);
    }
    assert(callback_count == count && disposition_count == count);
    printf("PASS: finite allowance %u never replenishes\n", count);
}

int main(void)
{
    LINK_INSTANCE link;
    bool complete;

    finite_allowance(1);
    finite_allowance(2);
    finite_allowance(10);

    initialize_link(&link);
    assert(link_set_receiver_credit(&link, 1) == 0);
    receive_attach(&link, 0);
    receive_transfer(&link, 0, false, false);
    assert(flow_count == 1 && disposition_count == 1);
    assert(link_set_receiver_credit(&link, 1) == 0);
    assert(flow_count == 2 && advertised_credit == 1);
    receive_transfer(&link, 1, false, false);
    assert(callback_count == 2 && disposition_count == 2 && flow_count == 2);
    puts("PASS: continuous consumption requires explicit credit after settlement");

    initialize_link(&link);
    assert(link_set_receiver_credit(&link, 1) == 0);
    receive_attach(&link, 0);
    receive_transfer(&link, 0, true, false);
    assert(callback_count == 0 && link.current_link_credit == 1);
    receive_transfer(&link, 0, false, true);
    assert(callback_count == 1 && last_payload_size == 4 && disposition_count == 1);
    assert(link.current_link_credit == 0 && flow_count == 1);
    puts("PASS: multipart delivery consumes one credit without replenishment");

    initialize_link(&link);
    assert(link_set_receiver_credit(&link, 2) == 0);
    receive_attach(&link, 0);
    assert(link_set_receiver_credit(&link, 0) == 0);
    receive_transfer(&link, 0, false, false);
    assert(link.current_link_credit == 0 && callback_count == 1 && disposition_count == 1);
    assert(advertised_credit == 0 && flow_count == 2);
    puts("PASS: transfer in flight after revocation cannot underflow or refill credit");

    initialize_link(&link);
    assert(link_set_receiver_credit(&link, 3) == 0);
    receive_attach(&link, 0);
    receive_transfer(&link, 0, false, false);
    assert(link_drain(&link) == 0);
    assert(advertised_drain && advertised_credit == 2 && !drain_complete(&link));
    assert(link_set_receiver_credit(&link, 1) != 0);
    receive_transfer(&link, 1, false, false);
    assert(callback_count == 2 && disposition_count == 2 && flow_count == 2);
    receive_flow(&link, 3, 0, false);
    assert(!drain_complete(&link));
    receive_flow(&link, 3, 1, true);
    assert(!drain_complete(&link));
    receive_flow(&link, 99, 0, true);
    assert(!drain_complete(&link) && link.delivery_count == 2);
    receive_flow(&link, 3, 0, true);
    assert(drain_complete(&link) && link.current_link_credit == 0 && link.delivery_count == 3);
    assert(link_set_receiver_credit(&link, 1) == 0);
    assert(!advertised_drain && !drain_complete(&link));
    puts("PASS: drain settles arrivals, validates returned credit, and permits a fresh grant");

    initialize_link(&link);
    assert(link_set_receiver_credit(&link, 1) == 0);
    receive_attach(&link, 0);
    receive_transfer(&link, 0, false, false);
    assert(link_drain(&link) == 0 && !drain_complete(&link));
    receive_flow(&link, 1, 0, true);
    assert(drain_complete(&link));
    puts("PASS: exhausted credit still waits for peer drain acknowledgement");

    initialize_link(&link);
    assert(link_set_receiver_credit(&link, 3) == 0);
    receive_attach(&link, UINT32_MAX - 1);
    receive_transfer(&link, 0, false, false);
    assert(link_drain(&link) == 0);
    receive_flow(&link, 1, 0, true);
    assert(drain_complete(&link) && link.delivery_count == 1);
    puts("PASS: drain return handles delivery-count wraparound");

    initialize_link(&link);
    assert(link_set_max_link_credit(&link, 2) == 0);
    receive_attach(&link, 0);
    receive_transfer(&link, 0, false, false);
    receive_transfer(&link, 1, false, false);
    assert(flow_count == 2 && advertised_credit == 2 && link.current_link_credit == 1);
    puts("PASS: legacy automatic credit behavior is unchanged");

    assert(link_set_receiver_credit(NULL, 1) != 0);
    assert(link_drain(NULL) != 0);
    assert(link_get_drain_complete(NULL, &complete) != 0);
    assert(link_get_drain_complete(&link, NULL) != 0);
    link.role = role_sender;
    assert(link_set_receiver_credit(&link, 1) != 0);
    assert(link_drain(&link) != 0);
    assert(link_get_drain_complete(&link, &complete) != 0);
    initialize_link(&link);
    assert(link_drain(&link) != 0);
    puts("PASS: invalid handles, roles, and unattached drains are rejected");
    return 0;
}
