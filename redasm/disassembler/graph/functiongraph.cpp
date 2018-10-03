#include "functiongraph.h"

namespace REDasm {
namespace Graphing {

FunctionGraph::FunctionGraph(ListingDocument* document): Graph(), m_document(document) { }
ListingDocument* FunctionGraph::document() { return m_document; }

void FunctionGraph::build(address_t address)
{
    this->buildNodes(address);
    this->buildEdges();
}

FunctionGraphData *FunctionGraph::vertexFromListingIndex(s64 index)
{
    for(auto& item : m_nodelist)
    {
       FunctionGraphData* v = static_cast<FunctionGraphData*>(item.get());

        if(v->contains(index))
            return v;
    }

    return NULL;
}

void FunctionGraph::buildNode(int index, FunctionGraph::IndexQueue &indexqueue)
{
    auto it = std::next(m_document->begin(), index);

    if(it == m_document->end())
        return;

    std::unique_ptr<FunctionGraphData> data = std::make_unique<FunctionGraphData>(index);
    ListingItem* item = it->get();

    for( ; it != m_document->end(); it++, index++)
    {
        item = it->get();

        if(this->vertexFromListingIndex(index))
            break;

        if(item->is(ListingItem::SymbolItem))
        {
            if(index == data->startidx) // Skip first label
                continue;

            SymbolPtr symbol = m_document->symbol(item->address);

            if(symbol->is(SymbolTypes::Code) && !symbol->isFunction())
                indexqueue.push(index);

            data->labelbreak = true;
            break;
        }

        if(!item->is(ListingItem::InstructionItem))
            break;

        data->endidx = index;
        InstructionPtr instruction = m_document->instruction(item->address);

        if(instruction->is(InstructionTypes::Jump))
        {
            for(address_t target : instruction->targets)
                indexqueue.push(m_document->indexOfSymbol(target));

            if(instruction->is(InstructionTypes::Conditional))
                indexqueue.push(index + 1);

            break;
        }

        if(instruction->is(InstructionTypes::Stop))
            break;
    }

    this->addNode(data.release());
}

void FunctionGraph::buildNodes(address_t startaddress)
{
    REDasm::ListingItem* item = m_document->functionStart(startaddress);

    if(!item)
        return;

    startaddress = item->address;

    IndexQueue queue;
    queue.push(m_document->indexOf(item) + 1); // Skip declaration

    while(!queue.empty())
    {
        s64 index = queue.front();
        queue.pop();

        if(index == -1)
            continue;

        item = m_document->functionStart(m_document->itemAt(index));

        if(!item || (item->address != startaddress) || this->vertexFromListingIndex(index))
            continue;

        this->buildNode(index, queue);
    }
}

void FunctionGraph::buildEdges()
{
    for(auto& item : m_nodelist)
    {
        FunctionGraphData* data = static_cast<FunctionGraphData*>(item.get());
        auto it = std::next(m_document->begin(), data->startidx);
        int index = data->startidx;

        if(data->labelbreak && (data->endidx + 1 < static_cast<s64>(m_document->size())))
            this->addEdge(data, this->vertexFromListingIndex(data->endidx + 1), ogdf::Color::Name::Blue);

        for( ; (it != m_document->end()) && (index <= data->endidx); it++, index++)
        {
            ListingItem* item = it->get();

            if(!item->is(ListingItem::InstructionItem))
                continue;

            InstructionPtr instruction = m_document->instruction(item->address);

            if(!instruction->is(InstructionTypes::Jump))
                continue;

            for(address_t target : instruction->targets)
            {
                int tgtindex = m_document->indexOfSymbol(target);
                FunctionGraphData* todata = this->vertexFromListingIndex(tgtindex);

                if(!todata)
                    continue;

                this->addEdge(data, todata, ogdf::Color::Name::Green);
            }

            if(instruction->is(InstructionTypes::Conditional))
            {
                FunctionGraphData* todata = this->vertexFromListingIndex(index + 1);

                if(!todata)
                    continue;

                this->addEdge(data, todata, ogdf::Color::Name::Red);
            }
        }
    }
}

} // namespace Graphing
} // namespace REDasm
