<?php

use Azure\uAMQP\Connection;
use Azure\uAMQP\Message;

require_once __DIR__ . '/emulator-parameters.php';

echo "Testing PHP uAMQP producer against Service Bus emulator\n";
echo "========================================================\n\n";

try {
    echo "1. Creating connection...\n";
    $conn = new Connection(
        PHUAMQP_HOST,
        PHUAMQP_PORT,
        PHUAMQP_USE_TLS,
        PHUAMQP_KEY_NAME,
        PHUAMQP_ACCESS_KEY,
        false
    );
    echo "✓ Connection created\n\n";

    echo "2. Sending messages to queue `" . PHUAMQP_QUEUE_NAME . "`...\n";
    for ($i = 1; $i <= PHUAMQP_MESSAGE_COUNT; $i++) {
        $payload = [
            'message' => $i,
            'source' => 'phuamqp-producer',
            'sentAt' => gmdate('c'),
        ];

        $message = new Message(json_encode($payload, JSON_THROW_ON_ERROR));

        echo sprintf("✓ Sending message %d/%d: %s\n", $i, PHUAMQP_MESSAGE_COUNT, $message->getBody());
        $conn->publish(PHUAMQP_QUEUE_NAME, $message);
    }

    $conn->close();
    echo "\n✓ Producer test completed successfully\n";
} catch (\Throwable $e) {
    echo "\n✗ ERROR: " . $e->getMessage() . "\n";
    exit(1);
}

