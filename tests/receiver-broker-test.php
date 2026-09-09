<?php

declare(strict_types=1);

use Azure\uAMQP\Connection;
use Azure\uAMQP\Message;

// Deliberately independent of production connection settings and examples/parameters.php.
$host = getenv('PHUAMQP_TEST_HOST') ?: 'phuamqp-servicebus-emulator';
if (!in_array($host, ['phuamqp-servicebus-emulator', 'localhost', '127.0.0.1'], true)) {
    throw new RuntimeException('Receiver regression must use the isolated local emulator');
}
$tracePath = tempnam(sys_get_temp_dir(), 'phuamqp-receiver-');
putenv('UAMQP_DEBUG_FILE=' . $tracePath);

function connection(bool $trace = true): Connection
{
    global $host;
    return new Connection($host, 5672, false, 'RootManageSharedAccessKey', 'EmulatorPassword123!', $trace);
}

function check(bool $condition, string $description): void
{
    if (!$condition) {
        throw new RuntimeException($description);
    }
}

function publishMessages(string $prefix, int $count): array
{
    $ids = [];
    $sender = connection(false);
    for ($index = 0; $index < $count; ++$index) {
        $id = $prefix . '-' . $index;
        $ids[] = $id;
        $message = new Message('synthetic:' . $id);
        $message->setMessageId($id);
        $message->setApplicationProperty('regression', 'S', $prefix);
        $sender->publish('test-queue', $message);
    }
    $sender->close();
    return $ids;
}

function receiveBatch(Connection $receiver, int $limit, ?callable $handler = null, string $queue = 'test-queue'): array
{
    $messages = [];
    $deadline = microtime(true) + 0.5;
    $receiver->setCallback(
        $queue,
        static function (Message $message) use (&$messages, $handler): mixed {
            $messages[] = $message;
            return $handler === null ? null : $handler($message, count($messages));
        },
        static function () use ($receiver, $deadline): void {
            while (!$receiver->wasCloseRequested() && microtime(true) < $deadline) {
                $receiver->consume();
                usleep(1000);
            }
        },
        $limit
    );
    return $messages;
}

try {
    $receiver = connection();
    check(receiveBatch($receiver, 1) === [], 'Test queue must initially be empty');
    $run = 'receiver-' . bin2hex(random_bytes(6));
    foreach ([1, 2, 10] as $batchSize) {
        $expected = publishMessages($run . '-batch' . $batchSize, 40);
        $actual = [];
        for ($index = 0; $index < 40 / $batchSize; ++$index) {
            $batch = receiveBatch($receiver, $batchSize);
            check(count($batch) === $batchSize, 'Finite batch count ' . $batchSize);
            foreach ($batch as $message) {
                $actual[] = $message->getMessageId();
                check($message->getBody() === 'synthetic:' . $message->getMessageId(), 'Message handle remains valid after callback');
                check($message->getDeliveryCount() === 0, 'Untouched backlog must not accumulate delivery attempts');
                check($message->getApplicationProperty('regression') === $run . '-batch' . $batchSize, 'Application metadata round trip');
                check($message->getMessageAnnotation('x-opt-sequence-number') !== null, 'Sequence number preserved');
            }
        }
        check($expected === $actual, 'All unique IDs completed for batch size ' . $batchSize);
        check(receiveBatch($receiver, 1) === [], 'No duplicate final message after batch ' . $batchSize);
        echo "Repeated batches of {$batchSize}: 40 unique messages completed\n";
    }

    $expected = publishMessages($run . '-early-stop', 5);
    $first = receiveBatch($receiver, 5, static fn (): bool => false);
    check(count($first) === 1, 'False callback must stop business delivery immediately');
    $remaining = receiveBatch($receiver, 4);
    check(array_merge([$first[0]->getMessageId()], array_map(static fn (Message $m) => $m->getMessageId(), $remaining)) === $expected,
        'Early-stop surplus must be released and recoverable');
    echo "Early stop releases surplus deliveries\n";

    $expected = publishMessages($run . '-close-callback', 3);
    $first = receiveBatch($receiver, 3, static function () use ($receiver): void {
        $receiver->close();
    });
    check(count($first) === 1, 'Explicit close inside callback must defer cleanup');
    $receiver = connection();
    $remaining = receiveBatch($receiver, 2);
    check(array_merge([$first[0]->getMessageId()], array_map(static fn (Message $m) => $m->getMessageId(), $remaining)) === $expected,
        'Callback close must not accept discarded work');
    echo "Explicit callback close preserves remaining messages\n";

    $receiver->setCallback('test-queue', static function (): void {
        throw new RuntimeException('Close-only loop must not deliver messages');
    }, static function () use ($receiver): void {
        $receiver->close();
    }, 1);
    $receiver = connection();
    echo "Close from the callback loop defers object destruction\n";

    $expected = publishMessages($run . '-retry', 5);
    $failed = false;
    $callbackCalls = 0;
    $failedId = null;
    $originalFailure = new RuntimeException('synthetic transient callback failure', 1739);
    try {
        receiveBatch($receiver, 5, static function (Message $message) use (&$callbackCalls, &$failedId, $originalFailure): void {
            ++$callbackCalls;
            $failedId = $message->getMessageId();
            throw $originalFailure;
        });
    } catch (Throwable $error) {
        check($error instanceof RuntimeException, 'Original callback exception class must be preserved');
        check($error->getCode() === 1739, 'Original callback exception code must be preserved');
        check($error === $originalFailure, 'Original callback exception object must be preserved');
        $failed = true;
    }
    check($failed, 'Original callback exception must reach PHP');
    check($callbackCalls === 1, 'Surplus callbacks must not run after an exception');
    $receiver = connection();
    $retry = receiveBatch($receiver, 5);
    $retriedIds = array_map(static fn (Message $message) => $message->getMessageId(), $retry);
    sort($expected);
    sort($retriedIds);
    check(count($retry) === 5 && $retriedIds === $expected, 'Failed delivery and all four surplus messages must remain recoverable');
    foreach ($retry as $message) {
        if ($message->getMessageId() === $failedId) {
            check($message->getDeliveryCount() >= 1, 'Abandon increments delivery count');
        }
    }
    echo "Original callback exception preserved; failed delivery and four surplus messages recovered\n";

    // Legacy three-argument API must remain a continuous receiver.
    $expected = publishMessages($run . '-continuous', 3);
    $actual = [];
    $deadline = microtime(true) + 3;
    $receiver->setCallback('test-queue', static function (Message $message) use (&$actual): bool {
        $actual[] = $message->getMessageId();
        return count($actual) < 3;
    }, static function () use ($receiver, $deadline): void {
        while (!$receiver->wasCloseRequested() && microtime(true) < $deadline) {
            $receiver->consume();
            usleep(1000);
        }
    });
    check($expected === $actual, 'Continuous receiver replenishes after settlement');
    check(receiveBatch($receiver, 10) === [], 'Empty receive drains unused credit');
    check(receiveBatch($receiver, 1, null, 'test-queue/$DeadLetterQueue') === [], 'No dead-letter messages');
    $receiver->close();

    $detached = false;
    $frame = '';
    foreach (file($tracePath, FILE_IGNORE_NEW_LINES) as $line) {
        if (str_contains($line, '"event":"receiver-created"')) {
            $detached = false;
        }
        if (preg_match('/log_outgoing_frame: \[([A-Z]+)\]/', $line, $matches)) {
            $frame = $matches[1];
            if ($frame === 'DETACH') {
                $detached = true;
            }
            check($frame !== 'DISPOSITION' || !$detached, 'Disposition must precede DETACH');
        } elseif (str_contains($line, 'log_outgoing_frame: * {') && $frame === 'FLOW') {
            $fields = explode(',', substr($line, strpos($line, '* {') + 3));
            check((int) ($fields[6] ?? 0) <= 10, 'Outgoing receive credit must respect batch allowance');
        }
    }
    echo "Receiver broker regression passed; trace: {$tracePath}\n";
} catch (Throwable $error) {
    fwrite(STDERR, $error->getMessage() . "\nTrace: {$tracePath}\n");
    exit(1);
}
