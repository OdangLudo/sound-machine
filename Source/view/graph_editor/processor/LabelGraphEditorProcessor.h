#pragma once

#include "BaseGraphEditorProcessor.h"

class LabelGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    LabelGraphEditorProcessor(StatefulAudioProcessorContainer &audioProcessorContainer, TracksState &tracks, ViewState &view,
                              const ValueTree &state, ConnectorDragListener &connectorDragListener);

    void resized() override;

private:
    DrawableText nameLabel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;
};
