# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
"""Client and server classes corresponding to protobuf-defined services."""
import grpc

from perfetto.bigtrace.protos.perfetto.bigtrace import orchestrator_pb2 as protos_dot_perfetto_dot_bigtrace_dot_orchestrator__pb2


class BigtraceOrchestratorStub(object):
  """gRPC Interface for a Bigtrace Orchestrator

    Each Bigtrace instance has an orchestrator which is responsible for receiving
    requests from the client and loading and querying traces by sharding them
    across a set of "Workers"
    """

  def __init__(self, channel):
    """Constructor.

        Args:
            channel: A grpc.Channel.
        """
    self.Query = channel.unary_stream(
        '/perfetto.protos.BigtraceOrchestrator/Query',
        request_serializer=protos_dot_perfetto_dot_bigtrace_dot_orchestrator__pb2
        .BigtraceQueryArgs.SerializeToString,
        response_deserializer=protos_dot_perfetto_dot_bigtrace_dot_orchestrator__pb2
        .BigtraceQueryResponse.FromString,
    )


class BigtraceOrchestratorServicer(object):
  """gRPC Interface for a Bigtrace Orchestrator

    Each Bigtrace instance has an orchestrator which is responsible for receiving
    requests from the client and loading and querying traces by sharding them
    across a set of "Workers"
    """

  def Query(self, request, context):
    """Executes a SQL query on the specified list of traces and returns a stream
        of the result of the query for a given trace
        """
    context.set_code(grpc.StatusCode.UNIMPLEMENTED)
    context.set_details('Method not implemented!')
    raise NotImplementedError('Method not implemented!')


def add_BigtraceOrchestratorServicer_to_server(servicer, server):
  rpc_method_handlers = {
      'Query':
          grpc.unary_stream_rpc_method_handler(
              servicer.Query,
              request_deserializer=protos_dot_perfetto_dot_bigtrace_dot_orchestrator__pb2
              .BigtraceQueryArgs.FromString,
              response_serializer=protos_dot_perfetto_dot_bigtrace_dot_orchestrator__pb2
              .BigtraceQueryResponse.SerializeToString,
          ),
  }
  generic_handler = grpc.method_handlers_generic_handler(
      'perfetto.protos.BigtraceOrchestrator', rpc_method_handlers)
  server.add_generic_rpc_handlers((generic_handler,))


# This class is part of an EXPERIMENTAL API.
class BigtraceOrchestrator(object):
  """gRPC Interface for a Bigtrace Orchestrator

    Each Bigtrace instance has an orchestrator which is responsible for receiving
    requests from the client and loading and querying traces by sharding them
    across a set of "Workers"
    """

  @staticmethod
  def Query(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
    return grpc.experimental.unary_stream(
        request, target, '/perfetto.protos.BigtraceOrchestrator/Query',
        protos_dot_perfetto_dot_bigtrace_dot_orchestrator__pb2.BigtraceQueryArgs
        .SerializeToString,
        protos_dot_perfetto_dot_bigtrace_dot_orchestrator__pb2
        .BigtraceQueryResponse.FromString, options, channel_credentials,
        insecure, call_credentials, compression, wait_for_ready, timeout,
        metadata)
