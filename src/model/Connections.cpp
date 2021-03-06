#include "Connections.h"

static bool canProcessorDefaultConnectTo(const Processor *processor, const Processor *otherProcessor, ConnectionType connectionType) {
    return processor == otherProcessor && processor->isProcessorAProducer(connectionType) && otherProcessor->isProcessorAnEffect(connectionType);
}

Processor *Connections::findDefaultDestinationProcessor(const Processor *sourceProcessor, ConnectionType connectionType) {
    if (sourceProcessor == nullptr || !sourceProcessor->isProcessorAProducer(connectionType)) return {};

    const auto *track = tracks.getTrackForProcessor(sourceProcessor);
    const auto *masterTrack = tracks.getMasterTrack();
    if (sourceProcessor->isTrackOutputProcessor()) {
        if (track == masterTrack) return {};
        if (masterTrack != nullptr) return masterTrack->getInputProcessor();
        return {};
    }

    bool isTrackInputProcessor = sourceProcessor->isTrackInputProcessor();
    const auto *lane = track->getProcessorLane();
    int siblingDelta = 0;
    ProcessorLane *otherLane;
    while ((otherLane = track->getProcessorLaneAt(siblingDelta++)) != nullptr) {
        for (auto *otherProcessor : otherLane->getChildren()) {
            if (otherProcessor == sourceProcessor) continue;
            bool isOtherProcessorBelow = isTrackInputProcessor || otherProcessor->getSlot() > sourceProcessor->getSlot();
            if (!isOtherProcessorBelow) continue;
            // If a non-effect (another producer) is under this processor in the same track, and no effect processors
            // to the right have a lower slot, block this producer's output by the other producer,
            // effectively replacing it.
            if (canProcessorDefaultConnectTo(sourceProcessor, otherProcessor, connectionType) || lane == otherLane) return otherProcessor;
        }
        // TODO adapt this when there are multiple lanes
        return track->getOutputProcessor();
    }

    return {};
}

bool Connections::isNodeConnected(AudioProcessorGraph::NodeID nodeId) const {
    for (const auto *connection : children) {
        if (connection->getSourceNodeId() == nodeId)
            return true;
    }
    return false;
}

static bool channelMatchesConnectionType(int channel, ConnectionType connectionType) {
    if (connectionType == all) return true;
    return (connectionType == audio && channel != AudioProcessorGraph::midiChannelIndex) ||
           (connectionType == midi && channel == AudioProcessorGraph::midiChannelIndex);
}

Array<fg::Connection *> Connections::getConnectionsForNode(const Processor *processor, ConnectionType connectionType, bool incoming, bool outgoing, bool includeCustom, bool includeDefault) {
    Array<fg::Connection *> nodeConnections;
    for (auto *connection : children) {
        if ((connection->isCustom() && !includeCustom) || (!connection->isCustom() && !includeDefault))
            continue;

        auto processorNodeId = processor->getNodeId();
        if ((incoming && connection->getDestinationNodeId() == processorNodeId && channelMatchesConnectionType(connection->getDestinationChannel(), connectionType)) ||
            (outgoing && connection->getSourceNodeId() == processorNodeId && channelMatchesConnectionType(connection->getSourceChannel(), connectionType)))
            nodeConnections.add(connection);
    }
    return nodeConnections;
}
